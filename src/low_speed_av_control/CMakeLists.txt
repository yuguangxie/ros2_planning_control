cmake_minimum_required(VERSION 3.8)
project(low_speed_av_control)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(chassis_interfaces REQUIRED)
find_package(low_speed_av_interfaces REQUIRED)

add_library(${PROJECT_NAME}
  src/front_ackermann_model.cpp
  src/dual_ackermann_model.cpp
  src/vehicle_model_factory.cpp
  src/pure_pursuit_controller.cpp
  src/stanley_controller.cpp
  src/lqr_controller.cpp
  src/mpc_sampler_controller.cpp
  src/controller_factory.cpp
  src/scu_command_mapper.cpp
  src/command_limiter.cpp
  src/command_smoother.cpp
  src/tracking_error_estimator.cpp
  src/control_node.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
ament_target_dependencies(${PROJECT_NAME}
  rclcpp
  builtin_interfaces
  geometry_msgs
  std_msgs
  chassis_interfaces
  low_speed_av_interfaces)

add_executable(control_node src/control_node_main.cpp)
target_link_libraries(control_node ${PROJECT_NAME})
ament_target_dependencies(control_node rclcpp)

install(TARGETS ${PROJECT_NAME} control_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_dependencies(rclcpp builtin_interfaces geometry_msgs std_msgs chassis_interfaces low_speed_av_interfaces)
ament_package()
