# 08 Testing Without ROS2

## Why

Codex environment does not have ROS2. Therefore this generation pack requires offline validation scripts in addition to ROS2 package files.

## Required Scripts

```text
scripts/validate_expected_tree.py
scripts/validate_sample_ad_package.py
scripts/offline_algorithm_smoke.py
```

## Script 1: validate_expected_tree.py

Checks that generated repository contains:

```text
src/low_speed_av_interfaces
src/low_speed_av_planning
src/low_speed_av_control
src/low_speed_av_bringup
config files
launch files
msg/srv files
reports
```

## Script 2: validate_sample_ad_package.py

Checks AD Package contract:

- manifest exists,
- required files exist,
- validation not failed,
- topology references are valid,
- waypoint ranges are valid,
- YAML waypoints contain all required fields,
- checksums match.

## Script 3: offline_algorithm_smoke.py

Pure Python algorithm smoke:

- load topology,
- Dijkstra from N0001 to N0003,
- A* from N0001 to N0003,
- stitch waypoints,
- compute a local horizon,
- compute a Pure Pursuit command,
- compute a Stanley command,
- verify output finite and limited.

## Real ROS2 Acceptance Later

After moving to real ROS2 machine:

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
colcon test --event-handlers console_direct+
colcon test-result --verbose
```

Codex must list these commands in final report but not claim they passed unless run.
