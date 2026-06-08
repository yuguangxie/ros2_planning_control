param(
  [switch]$ShowCommandsOnly
)

$ErrorActionPreference = "Stop"

$colconCmd = Get-Command "colcon" -ErrorAction SilentlyContinue
$ros2Cmd = Get-Command "ros2" -ErrorAction SilentlyContinue
$hasColcon = $null -ne $colconCmd
$hasRos2 = $null -ne $ros2Cmd

if ($hasColcon) {
  Write-Output "AVAILABLE: colcon -> $($colconCmd.Source)"
} else {
  Write-Output "SKIPPED_ROS2_UNAVAILABLE: colcon not found"
}

if ($hasRos2) {
  Write-Output "AVAILABLE: ros2 -> $($ros2Cmd.Source)"
} else {
  Write-Output "SKIPPED_ROS2_UNAVAILABLE: ros2 not found"
}

Write-Output ""
Write-Output "Later ROS2 verification commands:"
Write-Output "  colcon build"
Write-Output "  colcon test"
Write-Output "  colcon test-result --verbose"
Write-Output "  ros2 launch low_speed_av_bringup planning_control_demo.launch.py"
Write-Output "  ros2 service call /low_speed_av_planning/plan_route low_speed_av_interfaces/srv/PlanRoute ""{start_node_id: 'N0001', goal_node_id: 'N0003'}"""
Write-Output "  ros2 topic echo /planning/trajectory"
Write-Output "  ros2 topic pub /localization/pose geometry_msgs/msg/PoseStamped ""{header: {frame_id: 'map'}, pose: {position: {x: 0.0, y: 0.0, z: 0.0}, orientation: {w: 1.0}}}"""
Write-Output "  ros2 topic pub /safety/status low_speed_av_interfaces/msg/ModuleStatus ""{module_name: 'safety', state: 'estop', level: 2, message: 'test estop'}"""
Write-Output "  ros2 topic echo /yunle_chassis/control/scu_control_command"
Write-Output "  ros2 topic echo /control/command  # only when output.mode is both or internal"

if (-not $hasColcon -or -not $hasRos2) {
  Write-Output ""
  Write-Output "SKIPPED_ROS2_UNAVAILABLE: ROS2 integration commands were not executed in this environment."
  exit 0
}

if ($ShowCommandsOnly) {
  Write-Output "ROS2 tools are available, but ShowCommandsOnly was set; no build/test was run."
  exit 0
}

Write-Output "ROS2 tools appear available. Run the listed commands manually in a sourced ROS2 workspace."
