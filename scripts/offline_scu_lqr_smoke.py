#!/usr/bin/env python3
"""Offline SCU mapper and LQR behavior checks without ROS2."""
from __future__ import annotations

import math
from pathlib import Path


SHIFT_D = 1
SHIFT_N = 2
SHIFT_R = 3
GEAR_DRIVE = 1
GEAR_REVERSE = 2
GEAR_NEUTRAL = 4


def stop_command(stop_shift=SHIFT_D, torque_or_speed_mode=1):
    shift = stop_shift if stop_shift in {SHIFT_D, SHIFT_N, SHIFT_R} else SHIFT_D
    return {
        "scu_shift_level_request": shift,
        "scu_steering_angle_front": 0.0,
        "scu_steering_angle_rear": 0.0,
        "scu_target_speed": 0.0,
        "scu_brake_enable": True,
        "gw_left_turn_light_request": 0,
        "gw_right_turn_light_request": 0,
        "gw_position_light_request": 0,
        "gw_low_beam_request": 0,
        "scu_torque_or_speed_mode": torque_or_speed_mode,
        "steering_angle_speed_valid": False,
        "brake_force_command_valid": False,
    }


def scu_map(cmd, opts=None):
    opts = opts or {}
    max_steer = opts.get("max_steering_angle_deg", 30.0)
    max_speed = opts.get("max_target_speed_kmh", 5.0)
    front_sign = opts.get("front_steer_sign", 1.0)
    rear_sign = opts.get("rear_steer_sign", 1.0)
    stop_shift = opts.get("stop_shift_level", SHIFT_D)
    torque_mode = opts.get("torque_or_speed_mode", 1)
    warnings = []
    if cmd.get("emergency_stop") or cmd.get("brake", 0.0) > 0.0 or not cmd.get("enable", True):
        return stop_command(stop_shift, torque_mode), warnings
    gear_to_shift = {GEAR_DRIVE: SHIFT_D, GEAR_REVERSE: SHIFT_R, GEAR_NEUTRAL: SHIFT_N}
    shift = gear_to_shift.get(cmd.get("gear"))
    if shift not in {SHIFT_D, SHIFT_N, SHIFT_R}:
        warnings.append("invalid gear")
        return stop_command(stop_shift, torque_mode), warnings

    def steer(field, sign):
        deg = math.degrees(cmd.get(field, 0.0)) * sign
        if not math.isfinite(deg) or abs(deg) > max_steer:
            warnings.append(field)
            return 0.0
        return deg

    speed = abs(cmd.get("speed_mps", 0.0)) * 3.6
    if not math.isfinite(speed) or speed > max_speed:
        warnings.append("speed")
        speed = 0.0
    return {
        "scu_shift_level_request": shift,
        "scu_steering_angle_front": steer("front_steering_angle_rad", front_sign),
        "scu_steering_angle_rear": steer("rear_steering_angle_rad", rear_sign),
        "scu_target_speed": speed,
        "scu_brake_enable": False,
        "gw_left_turn_light_request": opts.get("left", 0),
        "gw_right_turn_light_request": opts.get("right", 0),
        "gw_position_light_request": opts.get("position", 0),
        "gw_low_beam_request": opts.get("low_beam", 0),
        "scu_torque_or_speed_mode": torque_mode,
        "steering_angle_speed_valid": opts.get("steering_angle_speed_valid", False),
        "brake_force_command_valid": opts.get("brake_force_command_valid", False),
    }, warnings


def front_ackermann(kappa, wheel_base=1.2, max_front=0.6):
    front = max(-max_front, min(max_front, math.atan(kappa * wheel_base)))
    return front, 0.0


def dual_ackermann(kappa, wheel_base=1.2, ratio=0.5, max_front=0.6, max_rear=0.6):
    tan_front = kappa * wheel_base / (1.0 + ratio)
    front = max(-max_front, min(max_front, math.atan(tan_front)))
    rear = max(-max_rear, min(max_rear, math.atan(-ratio * tan_front)))
    return front, rear


def normalize_angle(a):
    return math.atan2(math.sin(a), math.cos(a))


def lqr_delta(pose, trajectory, state_speed=0.5, opts=None):
    opts = opts or {}
    if not trajectory or all(abs(p["v"]) < 1e-3 for p in trajectory):
        return {"emergency_stop": True, "speed_mps": 0.0, "desired_curvature_1pm": 0.0}
    nearest = min(range(len(trajectory)), key=lambda i: math.hypot(trajectory[i]["x"] - pose[0], trajectory[i]["y"] - pose[1]))
    ref_speed = abs(trajectory[nearest]["v"])
    model_speed = max(opts.get("min_speed_mps", 0.2), abs(state_speed) if abs(state_speed) > 1e-6 else ref_speed)
    target_s = trajectory[nearest]["s"] + model_speed * opts.get("preview_time_s", 0.2)
    ref = trajectory[-1]
    for p in trajectory[nearest:]:
        if p["s"] >= target_s:
            ref = p
            break
    dx = pose[0] - ref["x"]
    dy = pose[1] - ref["y"]
    e_y = -math.sin(ref["yaw"]) * dx + math.cos(ref["yaw"]) * dy
    e_psi = normalize_angle(pose[2] - ref["yaw"])
    v = model_speed
    dt = max(opts.get("dt", 0.02), 1e-3)
    wb = max(opts.get("wheel_base", 1.2), 1e-6)
    A = (1.0, v * dt, 0.0, 1.0)
    B = (0.0, v * dt / wb)
    q0 = max(opts.get("q_lat", 3.0), 0.0)
    q1 = max(opts.get("q_yaw", 2.0), 0.0)
    r = max(opts.get("r", 1.0), 1e-9)
    p00, p01, p11 = q0, 0.0, q1
    for _ in range(int(opts.get("iterations", 80))):
        pb0, pb1 = p00 * B[0] + p01 * B[1], p01 * B[0] + p11 * B[1]
        den = max(r + B[0] * pb0 + B[1] * pb1, 1e-12)
        pa00, pa01 = p00 * A[0] + p01 * A[2], p00 * A[1] + p01 * A[3]
        pa10, pa11 = p01 * A[0] + p11 * A[2], p01 * A[1] + p11 * A[3]
        atpa00 = A[0] * pa00 + A[2] * pa10
        atpa01 = A[0] * pa01 + A[2] * pa11
        atpa11 = A[1] * pa01 + A[3] * pa11
        atpb0 = A[0] * pb0 + A[2] * pb1
        atpb1 = A[1] * pb0 + A[3] * pb1
        n00 = atpa00 - atpb0 * atpb0 / den + q0
        n01 = atpa01 - atpb0 * atpb1 / den
        n11 = atpa11 - atpb1 * atpb1 / den + q1
        if max(abs(n00 - p00), abs(n01 - p01), abs(n11 - p11)) < opts.get("eps", 1e-6):
            p00, p01, p11 = n00, n01, n11
            break
        p00, p01, p11 = n00, n01, n11
    pb0, pb1 = p00 * B[0] + p01 * B[1], p01 * B[0] + p11 * B[1]
    den = max(r + B[0] * pb0 + B[1] * pb1, 1e-12)
    pa00, pa01 = p00 * A[0] + p01 * A[2], p00 * A[1] + p01 * A[3]
    pa10, pa11 = p01 * A[0] + p11 * A[2], p01 * A[1] + p11 * A[3]
    k0 = (B[0] * pa00 + B[1] * pa10) / den
    k1 = (B[0] * pa01 + B[1] * pa11) / den
    ff = math.atan(wb * ref["kappa"]) if opts.get("feedforward", True) else 0.0
    delta = ff - (k0 * e_y + k1 * e_psi)
    max_delta = abs(opts.get("max_delta", 0.52))
    delta = max(-max_delta, min(max_delta, delta))
    return {
        "emergency_stop": False,
        "speed_mps": ref["v"],
        "desired_curvature_1pm": math.tan(delta) / wb,
        "steering_angle_rad": delta,
        "gear": ref.get("gear", GEAR_DRIVE),
    }


def limit_and_smooth(cmd):
    if not all(math.isfinite(cmd.get(k, 0.0)) for k in ["speed_mps", "desired_curvature_1pm", "front_steering_angle_rad", "rear_steering_angle_rad"]):
        return {
            "speed_mps": 0.0,
            "front_steering_angle_rad": 0.0,
            "rear_steering_angle_rad": 0.0,
            "brake": 1.0,
            "emergency_stop": True,
        }
    cmd["speed_mps"] = max(-1.2, min(1.2, cmd.get("speed_mps", 0.0)))
    cmd["front_steering_angle_rad"] = max(-0.6, min(0.6, cmd.get("front_steering_angle_rad", 0.0)))
    cmd["rear_steering_angle_rad"] = max(-0.6, min(0.6, cmd.get("rear_steering_angle_rad", 0.0)))
    return cmd


def test_scu_mapper():
    cmd = {"speed_mps": 1.0, "front_steering_angle_rad": math.radians(10), "rear_steering_angle_rad": math.radians(-5), "gear": GEAR_DRIVE, "enable": True}
    msg, warnings = scu_map(cmd)
    assert not warnings
    assert msg["scu_shift_level_request"] == SHIFT_D
    assert abs(msg["scu_target_speed"] - 3.6) < 1e-9
    assert abs(msg["scu_steering_angle_front"] - 10.0) < 1e-9
    reverse = dict(cmd, gear=GEAR_REVERSE, speed_mps=-0.5)
    msg, _ = scu_map(reverse)
    assert msg["scu_shift_level_request"] == SHIFT_R and abs(msg["scu_target_speed"] - 1.8) < 1e-9
    neutral = dict(cmd, gear=GEAR_NEUTRAL, speed_mps=0.0)
    msg, _ = scu_map(neutral)
    assert msg["scu_shift_level_request"] == SHIFT_N
    msg, warnings = scu_map(dict(cmd, gear=99))
    assert msg["scu_shift_level_request"] == SHIFT_D and msg["scu_brake_enable"] and warnings
    msg, _ = scu_map(cmd, {"front_steer_sign": -1.0, "rear_steer_sign": -1.0})
    assert abs(msg["scu_steering_angle_front"] + 10.0) < 1e-9
    msg, warnings = scu_map(dict(cmd, front_steering_angle_rad=float("nan")))
    assert msg["scu_steering_angle_front"] == 0.0 and warnings
    msg, warnings = scu_map(dict(cmd, front_steering_angle_rad=math.radians(40)))
    assert msg["scu_steering_angle_front"] == 0.0 and warnings
    msg, warnings = scu_map(dict(cmd, speed_mps=float("inf")))
    assert msg["scu_target_speed"] == 0.0 and warnings
    msg, warnings = scu_map(dict(cmd, speed_mps=2.0), {"max_target_speed_kmh": 5.0})
    assert msg["scu_target_speed"] == 0.0 and warnings
    msg, _ = scu_map(dict(cmd, emergency_stop=True, brake=1.0))
    assert msg["scu_brake_enable"] and msg["scu_target_speed"] == 0.0
    assert msg["steering_angle_speed_valid"] is False and msg["brake_force_command_valid"] is False


def test_ackermann_and_lqr():
    front, rear = front_ackermann(0.1)
    assert math.isfinite(front) and rear == 0.0 and abs(front) <= 0.6
    front, rear = dual_ackermann(0.1)
    assert math.isfinite(front) and math.isfinite(rear) and abs(front) <= 0.6 and abs(rear) <= 0.6
    traj = [{"x": i * 0.5, "y": 0.0, "yaw": 0.0, "kappa": 0.0, "v": 0.5, "s": i * 0.5, "gear": GEAR_DRIVE} for i in range(8)]
    left = lqr_delta((0.0, 0.2, 0.0), traj)
    right = lqr_delta((0.0, -0.2, 0.0), traj)
    assert left["steering_angle_rad"] < 0.0 and right["steering_angle_rad"] > 0.0
    curved = [{"x": 0.0, "y": 0.0, "yaw": 0.0, "kappa": 0.1, "v": 0.5, "s": 0.0, "gear": GEAR_DRIVE}]
    ff = lqr_delta((0.0, 0.0, 0.0), curved)
    assert ff["steering_angle_rad"] > 0.0
    q1 = lqr_delta((0.0, 0.2, 0.0), traj, opts={"q_lat": 3.0})
    q2 = lqr_delta((0.0, 0.2, 0.0), traj, opts={"q_lat": 8.0})
    assert abs(q1["steering_angle_rad"] - q2["steering_angle_rad"]) > 1e-6
    low = lqr_delta((0.0, 0.1, 0.0), traj, state_speed=0.0)
    assert math.isfinite(low["steering_angle_rad"])
    stop = lqr_delta((0.0, 0.0, 0.0), [{"x": 0.0, "y": 0.0, "yaw": 0.0, "kappa": 0.0, "v": 0.0, "s": 0.0}])
    assert stop["emergency_stop"]
    for model in (front_ackermann, dual_ackermann):
        front, rear = model(left["desired_curvature_1pm"])
        out = limit_and_smooth(dict(left, front_steering_angle_rad=front, rear_steering_angle_rad=rear))
        assert math.isfinite(out["front_steering_angle_rad"]) and math.isfinite(out["rear_steering_angle_rad"])


def static_source_checks():
    cmake = Path("src/low_speed_av_control/CMakeLists.txt").read_text(encoding="utf-8")
    package = Path("src/low_speed_av_control/package.xml").read_text(encoding="utf-8")
    node = Path("src/low_speed_av_control/src/control_node.cpp").read_text(encoding="utf-8")
    lqr = Path("src/low_speed_av_control/src/lqr_controller.cpp").read_text(encoding="utf-8")
    mapper = Path("src/low_speed_av_control/src/scu_command_mapper.cpp").read_text(encoding="utf-8")
    assert "find_package(chassis_interfaces REQUIRED)" in cmake
    assert "<depend>chassis_interfaces</depend>" in package
    assert "create_publisher<chassis_interfaces::msg::ScuControlCommand>" in node
    assert "/yunle_chassis/control/scu_control_command" in node
    assert "desired_curvature_1pm" in node and "steering_from_curvature(raw.desired_curvature_1pm" in node
    assert "p_next00" in lqr and "lqr_tracking" in lqr and "lqr_experimental" not in lqr
    assert "scu_shift_level_request" in mapper and "internal gear is unknown" in mapper


def main():
    test_scu_mapper()
    test_ackermann_and_lqr()
    static_source_checks()
    print("Offline SCU/LQR smoke OK: SCU mapping safe, Ackermann finite, Riccati LQR finite/config-sensitive")


if __name__ == "__main__":
    main()
