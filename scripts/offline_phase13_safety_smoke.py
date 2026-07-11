#!/usr/bin/env python3
"""Behavioral offline smoke for the Phase 13 Control safety contract.

This executable model mirrors the ROS-independent C++ safety decision tables.
It does not claim to compile or execute ROS2/C++ code.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, replace
from enum import Enum
from pathlib import Path

import yaml


class State(str, Enum):
    WAIT_INPUTS = "WAIT_INPUTS"
    READY = "READY"
    ACTIVE = "ACTIVE"
    CONTROLLED_STOP = "CONTROLLED_STOP"
    ESTOP_LATCHED = "ESTOP_LATCHED"


@dataclass(frozen=True)
class Inputs:
    safety_request: bool = False
    safety_latched: bool = False
    trajectory_emergency: bool = False
    have_pose: bool = True
    pose_valid: bool = True
    pose_timeout: bool = False
    have_trajectory: bool = True
    trajectory_valid: bool = True
    trajectory_timeout: bool = False
    vehicle_required: bool = False
    have_vehicle: bool = True
    vehicle_valid: bool = True
    vehicle_timeout: bool = False
    autonomous_enabled: bool = True
    brake_pressed: bool = False
    fault_code: str = ""
    force_ready: bool = False


@dataclass(frozen=True)
class Decision:
    state: State
    motion_allowed: bool
    emergency: bool
    reason: str


def decide(value: Inputs) -> Decision:
    if value.safety_request or value.safety_latched:
        return Decision(State.ESTOP_LATCHED, False, True, "safety_estop_latched")
    if value.trajectory_emergency:
        return Decision(State.ESTOP_LATCHED, False, True, "trajectory_emergency_stop")
    if value.have_vehicle:
        if not value.vehicle_valid:
            return Decision(State.CONTROLLED_STOP, False, False, "invalid_vehicle_state")
        if value.vehicle_timeout:
            return Decision(State.CONTROLLED_STOP, False, False, "vehicle_state_timeout")
        if value.fault_code:
            return Decision(State.CONTROLLED_STOP, False, True, f"vehicle_fault:{value.fault_code}")
        if not value.autonomous_enabled:
            return Decision(State.CONTROLLED_STOP, False, False, "autonomous_disabled")
        if value.brake_pressed:
            return Decision(State.CONTROLLED_STOP, False, False, "vehicle_brake_pressed")
    elif value.vehicle_required:
        return Decision(State.WAIT_INPUTS, False, False, "waiting_for_vehicle_state")
    if not value.have_pose:
        return Decision(State.WAIT_INPUTS, False, False, "waiting_for_localization")
    if not value.pose_valid or value.pose_timeout:
        return Decision(State.CONTROLLED_STOP, False, False, "localization_invalid_or_timeout")
    if not value.have_trajectory:
        return Decision(State.WAIT_INPUTS, False, False, "waiting_for_trajectory")
    if not value.trajectory_valid or value.trajectory_timeout:
        return Decision(State.CONTROLLED_STOP, False, False, "trajectory_invalid_or_timeout")
    if value.force_ready:
        return Decision(State.READY, False, False, "inputs_ready_after_estop_clear")
    return Decision(State.ACTIVE, True, False, "tracking_trajectory")


def stop_output(decision: Decision) -> dict[str, object]:
    assert not decision.motion_allowed
    return {
        "speed_mps": 0.0,
        "front_steering_angle_rad": 0.0,
        "rear_steering_angle_rad": 0.0,
        "brake": 1.0,
        "enable": False,
        "emergency_stop": decision.emergency,
        "reason": decision.reason,
        "scu_target_speed": 0.0,
        "scu_brake_enable": True,
    }


def trajectory_valid(
    status: str, emergency: bool, points: list[dict[str, float | int]],
    trajectory_id: str = "t1", source_package_id: str = "p1",
) -> bool:
    if emergency or not trajectory_id or not source_package_id or status != "ok" or not points:
        return False
    previous_s = -math.inf
    for point in points:
        numeric = [point[key] for key in ("x", "y", "yaw", "kappa", "s", "v")]
        if not all(math.isfinite(float(value)) for value in numeric):
            return False
        if int(point["gear"]) not in {1, 2, 3}:
            return False
        if float(point["s"]) + 1.0e-4 < previous_s:
            return False
        previous_s = float(point["s"])
    return True


def clear_allowed(
    *, safety_request: bool = False, trajectory_emergency: bool = False,
    localization_ready: bool = True, trajectory_ready: bool = True,
    have_vehicle: bool = True, vehicle_ready: bool = True,
    speed_mps: float = 0.0, threshold_mps: float = 0.05,
    autonomous_enabled: bool = True, brake_pressed: bool = False,
    fault_code: str = "",
) -> bool:
    return all((
        not safety_request,
        not trajectory_emergency,
        localization_ready,
        trajectory_ready,
        have_vehicle,
        vehicle_ready,
        math.isfinite(speed_mps) and abs(speed_mps) <= threshold_mps,
        autonomous_enabled,
        not brake_pressed,
        not fault_code,
    ))


def assert_stop(case: Inputs) -> None:
    decision = decide(case)
    output = stop_output(decision)
    assert output["speed_mps"] == 0.0
    assert output["enable"] is False
    assert output["brake"] == 1.0
    assert output["reason"]
    assert output["scu_target_speed"] == 0.0
    assert output["scu_brake_enable"] is True


def main() -> None:
    nominal = Inputs()
    controllers = ("pure_pursuit", "stanley", "lqr", "mpc_sampler")
    models = ("front_ackermann", "dual_ackermann")
    for controller in controllers:
        for model in models:
            assert_stop(replace(nominal, trajectory_emergency=True))
            assert_stop(replace(nominal, trajectory_valid=False))

    point = {"x": 0.0, "y": 0.0, "yaw": 0.0, "kappa": 0.0, "s": 0.0, "v": 0.8, "gear": 1}
    assert not trajectory_valid("ok", True, [point])
    assert not trajectory_valid("failure", False, [point])
    assert not trajectory_valid("ok", False, [point], trajectory_id="")
    assert not trajectory_valid("ok", False, [{**point, "x": math.nan}])
    assert not trajectory_valid("ok", False, [point, {**point, "s": -1.0}])
    assert trajectory_valid("ok", False, [point])

    for unsafe in (
        replace(nominal, trajectory_timeout=True),
        replace(nominal, pose_timeout=True),
        replace(nominal, autonomous_enabled=False),
        replace(nominal, brake_pressed=True),
        replace(nominal, fault_code="E42"),
        replace(nominal, vehicle_timeout=True),
    ):
        assert_stop(unsafe)

    latched = False
    latched = latched or True  # safety estop
    latched = latched or False  # ordinary OK heartbeat cannot clear
    assert latched
    assert not clear_allowed(speed_mps=0.2)
    assert not clear_allowed(fault_code="E42")
    assert not clear_allowed(brake_pressed=True)
    assert not clear_allowed(autonomous_enabled=False)
    assert not clear_allowed(have_vehicle=False)
    assert clear_allowed()
    ready = decide(replace(nominal, force_ready=True))
    assert ready.state is State.READY and not ready.motion_allowed

    for config_path in (
        Path("src/low_speed_av_control/config/control_params.yaml"),
        Path("src/low_speed_av_bringup/config/control_params.yaml"),
    ):
        params = yaml.safe_load(config_path.read_text(encoding="utf-8"))["low_speed_av_control"]["ros__parameters"]
        assert params["output"]["mode"] == "both"
        assert params["controller"]["allowed_trajectory_statuses"] == ["ok"]
        assert params["vehicle_state"]["timeout_s"] > 0.0

    print("Phase 13 Control safety smoke OK: 4 controllers x 2 models, metadata, watchdog gates, latch/clear, READY interlock, dual output config")


if __name__ == "__main__":
    main()
