cmake_minimum_required(VERSION 3.8)
project(low_speed_av_bringup)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch config sample_ad_package
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(launch_testing_ament_cmake REQUIRED)
  add_launch_test(test/test_planning_control_safety_launch.py TIMEOUT 60)
endif()

ament_package()
