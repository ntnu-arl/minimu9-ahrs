cmake_minimum_required(VERSION 2.8.3)
project(minimu9-ahrs)

find_package(catkin REQUIRED COMPONENTS
  message_runtime
  roscpp
  rospy
  sensor_msgs
  std_msgs
  tf
)

find_package(Eigen3 REQUIRED)
catkin_package(
)

include_directories(
  ${catkin_INCLUDE_DIRS}
)

add_compile_options(-std=c++11)

include_directories(include ${EIGEN3_INCLUDE_DIR} 
    include/minimu9-ahrs 
    src/minimu9-ahrs 
    ${catkin_INCLUDE_DIRS}
)

add_executable( minimu9-ahrs-ros 
  src/i2c_bus.cpp
  src/l3g.cpp
  src/lis3mdl.cpp
  src/lsm303.cpp
  src/lsm6.cpp
  src/main.cpp
  src/minimu9.cpp
)
  
target_link_libraries(minimu9-ahrs-ros 
    ${catkin_LIBRARIES}
)