cmake_minimum_required(VERSION 3.8)
project(low_speed_av_planning)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(low_speed_av_interfaces REQUIRED)
find_package(yaml-cpp REQUIRED)

add_library(${PROJECT_NAME}
  src/roadnet_loader.cpp
  src/topology_graph.cpp
  src/dijkstra_planner.cpp
  src/astar_planner.cpp
  src/global_planner_factory.cpp
  src/reference_line_motion_planner.cpp
  src/stop_and_wait_motion_planner.cpp
  src/frenet_lite_motion_planner.cpp
  src/hybrid_astar_parking_planner.cpp
  src/constant_speed_planner.cpp
  src/curvature_speed_planner.cpp
  src/obstacle_aware_speed_planner.cpp
  src/planning_node.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
ament_target_dependencies(${PROJECT_NAME} rclcpp geometry_msgs std_msgs low_speed_av_interfaces)
target_link_libraries(${PROJECT_NAME} yaml-cpp)

add_executable(planning_node src/planning_node_main.cpp)
target_link_libraries(planning_node ${PROJECT_NAME})
ament_target_dependencies(planning_node rclcpp)

install(TARGETS ${PROJECT_NAME} planning_node
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
  INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp geometry_msgs std_msgs low_speed_av_interfaces yaml-cpp)
ament_package()
