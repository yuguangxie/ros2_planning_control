#!/usr/bin/env python3
"""Offline model tests for simulated localization path following.

The ROS2 node publishes PoseStamped at runtime. This script checks the same
path-follow contracts without ROS2: initial pose, path interpolation, duplicate
republish de-duplication, reanchor on new path, failure-stop hold, reverse
geometry following, quaternion validity, and local-trajectory fallback.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


@dataclass
class Point:
    x: float
    y: float
    yaw: float
    speed: float = 1.0
    gear: int = 1
    waypoint_id: str = ""
    edge_id: str = ""
    behavior: str = ""


def distance(a: Point, b: Point) -> float:
    return math.hypot(a.x - b.x, a.y - b.y)


def quaternion_from_yaw(yaw: float) -> tuple[float, float, float, float]:
    z = math.sin(yaw * 0.5)
    w = math.cos(yaw * 0.5)
    norm = math.sqrt(z * z + w * w)
    return (0.0, 0.0, z / norm, w / norm)


def quat_valid(q: tuple[float, float, float, float]) -> bool:
    return all(math.isfinite(v) for v in q) and abs(sum(v * v for v in q) - 1.0) < 1.0e-9


def total_length(points: list[Point]) -> float:
    return sum(distance(points[i - 1], points[i]) for i in range(1, len(points)))


def signature(points: list[Point], status: str = "ok", emergency_stop: bool = False) -> str:
    if not points:
        return f"{status}:empty"
    return (
        f"{status}|{emergency_stop}|{len(points)}|"
        f"{points[0].waypoint_id}|{points[0].x:.3f},{points[0].y:.3f}|"
        f"{points[-1].waypoint_id}|{points[-1].x:.3f},{points[-1].y:.3f}|"
        f"{total_length(points):.3f}"
    )


def sample(points: list[Point], s: float) -> Point:
    if len(points) == 1:
        return points[0]
    acc = 0.0
    for i in range(1, len(points)):
        seg = distance(points[i - 1], points[i])
        if acc + seg >= s:
            ratio = 0.0 if seg < 1.0e-9 else (s - acc) / seg
            yaw = points[i - 1].yaw + (points[i].yaw - points[i - 1].yaw) * ratio
            return Point(
                x=points[i - 1].x + (points[i].x - points[i - 1].x) * ratio,
                y=points[i - 1].y + (points[i].y - points[i - 1].y) * ratio,
                yaw=yaw,
                speed=points[i - 1].speed,
                gear=points[i - 1].gear,
            )
        acc += seg
    return points[-1]


def nearest_s(points: list[Point], pose: Point) -> float:
    best_s = 0.0
    best_d = float("inf")
    acc = 0.0
    for i in range(1, len(points)):
        a, b = points[i - 1], points[i]
        vx, vy = b.x - a.x, b.y - a.y
        seg_sq = vx * vx + vy * vy
        t = 0.0 if seg_sq < 1.0e-9 else max(0.0, min(1.0, ((pose.x - a.x) * vx + (pose.y - a.y) * vy) / seg_sq))
        px, py = a.x + vx * t, a.y + vy * t
        d = math.hypot(pose.x - px, pose.y - py)
        if d < best_d:
            best_d = d
            best_s = acc + math.sqrt(seg_sq) * t
        acc += math.sqrt(seg_sq)
    return best_s


class OfflineFollower:
    def __init__(self) -> None:
        self.pose = Point(0.554, 1.473, -0.9178)
        self.path: list[Point] = []
        self.path_signature = ""
        self.progress = 0.0
        self.status = "waiting_for_path"
        self.source = ""
        self.speed = 0.0

    def receive_path(
        self,
        points: list[Point],
        *,
        source: str,
        status: str = "ok",
        emergency_stop: bool = False,
        reanchor: bool = True,
    ) -> None:
        if emergency_stop or "failure" in status or any("failure_stop" in p.behavior for p in points):
            self.status = "holding_failure_stop"
            self.speed = 0.0
            return
        if len(points) < 2:
            self.status = "invalid_path"
            self.speed = 0.0
            return
        sig = signature(points, status, emergency_stop)
        if sig == self.path_signature:
            return
        if self.source == "full_reference_path" and source == "trajectory":
            return
        self.path = points
        self.path_signature = sig
        self.source = source
        self.progress = nearest_s(points, self.pose) if reanchor else 0.0
        self.status = "following_path"

    def step(self, dt: float, max_speed: float = 1.0) -> None:
        if self.status != "following_path" or not self.path:
            return
        idx_s = min(self.progress, total_length(self.path))
        p = sample(self.path, idx_s)
        target_speed = max(0.0, min(max_speed, p.speed if p.speed > 1.0e-3 else max_speed))
        self.speed = target_speed
        self.progress += self.speed * dt
        length = total_length(self.path)
        if self.progress >= length:
            self.progress = length
            self.status = "arrived"
            self.speed = 0.0
        self.pose = sample(self.path, self.progress)


def assert_close(value: float, expected: float, tolerance: float, label: str) -> None:
    assert abs(value - expected) <= tolerance, f"{label}: expected {expected}, got {value}"


def main() -> int:
    follower = OfflineFollower()
    assert_close(follower.pose.x, 0.554, 1.0e-9, "initial x")
    assert quat_valid(quaternion_from_yaw(follower.pose.yaw))

    full = [
        Point(0.0, 0.0, 0.0, waypoint_id="A"),
        Point(1.0, 0.0, 0.0, waypoint_id="B"),
        Point(2.0, 0.0, 0.0, waypoint_id="C"),
    ]
    follower.pose = Point(0.0, 0.0, 0.0)
    follower.receive_path(full, source="full_reference_path")
    follower.step(0.5)
    assert follower.pose.x > 0.0, "pose did not advance on full reference path"
    progress_after_first_step = follower.progress
    follower.receive_path(full, source="full_reference_path")
    assert_close(follower.progress, progress_after_first_step, 1.0e-9, "duplicate republish reset progress")

    new_path = [
        Point(0.4, 0.0, 0.0, waypoint_id="D"),
        Point(2.0, 0.0, 0.0, waypoint_id="E"),
        Point(3.0, 0.0, 0.0, waypoint_id="F"),
    ]
    follower.receive_path(new_path, source="full_reference_path")
    assert follower.progress > 0.0, "new path did not reanchor from current pose"

    follower.receive_path([Point(0.0, 0.0, 0.0, behavior="planning_failure_stop")], source="trajectory")
    assert follower.status == "holding_failure_stop"
    held_x = follower.pose.x
    follower.step(1.0)
    assert_close(follower.pose.x, held_x, 1.0e-9, "failure_stop did not hold pose")

    reverse = [
        Point(2.0, 0.0, math.pi, speed=0.5, gear=2, waypoint_id="R1"),
        Point(1.0, 0.0, math.pi, speed=0.5, gear=2, waypoint_id="R2"),
        Point(0.0, 0.0, math.pi, speed=0.0, gear=2, waypoint_id="R3"),
    ]
    follower = OfflineFollower()
    follower.pose = Point(2.0, 0.0, math.pi)
    follower.receive_path(reverse, source="full_reference_path")
    follower.step(1.0)
    assert follower.pose.x < 2.0, "reverse path did not move along published geometry"
    assert quat_valid(quaternion_from_yaw(follower.pose.yaw))

    follower = OfflineFollower()
    follower.pose = Point(0.0, 0.0, 0.0)
    follower.receive_path(full, source="trajectory")
    follower.step(0.2)
    assert follower.source == "trajectory" and follower.pose.x > 0.0, "trajectory fallback did not work"

    for _ in range(20):
        follower.step(0.2)
    assert follower.status == "arrived", "follower did not stop at goal"
    goal_x = follower.pose.x
    follower.step(1.0)
    assert_close(follower.pose.x, goal_x, 1.0e-9, "arrived pose did not hold")

    print("offline_sim_localization_follow_smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
