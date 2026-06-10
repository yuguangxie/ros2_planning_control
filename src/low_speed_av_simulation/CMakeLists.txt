cmake_minimum_required(VERSION 3.8)
project(low_speed_av_simulation)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(low_speed_av_interfaces REQUIRED)
find_package(low_speed_av_planning REQUIRED)

add_executable(roadnet_visualization_node src/roadnet_visualization_node.cpp)
target_compile_features(roadnet_visualization_node PUBLIC cxx_std_17)
ament_target_dependencies(roadnet_visualization_node
  rclcpp
  geometry_msgs
  nav_msgs
  std_msgs
  visualization_msgs
  low_speed_av_interfaces
  low_speed_av_planning)

add_executable(sim_localization_pose_publisher_node src/sim_localization_pose_publisher_node.cpp)
target_compile_features(sim_localization_pose_publisher_node PUBLIC cxx_std_17)
ament_target_dependencies(sim_localization_pose_publisher_node
  rclcpp
  geometry_msgs
  std_srvs
  low_speed_av_interfaces
  low_speed_av_planning)

install(TARGETS
  roadnet_visualization_node
  sim_localization_pose_publisher_node
  RUNTIME DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY config launch rviz
  DESTINATION share/${PROJECT_NAME})

ament_package()
