cmake_minimum_required(VERSION 3.8)
project(low_speed_av_simulation)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(low_speed_av_interfaces REQUIRED)
find_package(low_speed_av_planning REQUIRED)

add_library(low_speed_av_simulation_core SHARED
  src/kinematic_vehicle_plant.cpp
  src/simulation_runtime_monitor.cpp)
target_compile_features(low_speed_av_simulation_core PUBLIC cxx_std_17)
target_include_directories(low_speed_av_simulation_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

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
  diagnostic_msgs
  geometry_msgs
  nav_msgs
  std_srvs
  low_speed_av_interfaces
  low_speed_av_planning)
target_link_libraries(sim_localization_pose_publisher_node low_speed_av_simulation_core)

install(TARGETS
  low_speed_av_simulation_core
  roadnet_visualization_node
  sim_localization_pose_publisher_node
  EXPORT export_low_speed_av_simulation_core
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY config launch rviz
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(launch_testing_ament_cmake REQUIRED)
  ament_add_gtest(test_simulation_core test/test_simulation_core.cpp)
  target_link_libraries(test_simulation_core low_speed_av_simulation_core)
  add_launch_test(test/test_sim_localization_reset_launch.py TIMEOUT 60)
  add_launch_test(test/test_control_closed_loop_plant_launch.py TIMEOUT 60)
endif()

ament_export_include_directories(include)
ament_export_targets(export_low_speed_av_simulation_core HAS_LIBRARY_TARGET)

ament_package()
