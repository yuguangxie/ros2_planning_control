cmake_minimum_required(VERSION 3.8)
project(low_speed_av_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/TrajectoryPoint.msg"
  "msg/Trajectory.msg"
  "msg/GlobalRoute.msg"
  "msg/ControlCommand.msg"
  "msg/VehicleState.msg"
  "msg/ModuleStatus.msg"
  "msg/RoadnetStatus.msg"
  "srv/ReloadRoadnet.srv"
  "srv/PlanRoute.srv"
  "srv/PlanMission.srv"
  "srv/SetPlannerAlgorithm.srv"
  "srv/SetControllerAlgorithm.srv"
  DEPENDENCIES std_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
