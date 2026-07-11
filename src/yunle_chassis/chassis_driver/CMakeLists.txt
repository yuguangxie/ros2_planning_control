cmake_minimum_required(VERSION 3.8)
project(chassis_driver)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(chassis_interfaces REQUIRED)

include_directories(include)

add_library(chassis_driver_core
  src/dbc_protocol.cpp
  src/can_ethernet_codec.cpp
  src/scu_control_frame_builder.cpp
)
target_include_directories(chassis_driver_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(chassis_driver_core PUBLIC cxx_std_17)

add_executable(chassis_driver_node
  src/main.cpp
  src/chassis_driver_node.cpp
  src/control_command_bridge.cpp
  src/frame_router.cpp
  src/udp_channel.cpp
)
target_link_libraries(chassis_driver_node chassis_driver_core)

add_executable(keyboard_scu_control_node
  src/keyboard_scu_control_node.cpp
)

ament_target_dependencies(chassis_driver_node
  rclcpp
  std_msgs
  builtin_interfaces
  chassis_interfaces
)

ament_target_dependencies(keyboard_scu_control_node
  rclcpp
  chassis_interfaces
)

install(TARGETS chassis_driver_core
  EXPORT export_chassis_driver_core
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include)

install(TARGETS chassis_driver_node keyboard_scu_control_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_chassis_core test/test_chassis_core.cpp)
  target_link_libraries(test_chassis_core chassis_driver_core)
endif()

ament_export_include_directories(include)
ament_export_targets(export_chassis_driver_core HAS_LIBRARY_TARGET)
ament_export_libraries(chassis_driver_core)

ament_package()
