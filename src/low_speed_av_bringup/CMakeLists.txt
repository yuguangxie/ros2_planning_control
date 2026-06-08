cmake_minimum_required(VERSION 3.8)
project(low_speed_av_bringup)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch config sample_ad_package
  DESTINATION share/${PROJECT_NAME})

ament_package()
