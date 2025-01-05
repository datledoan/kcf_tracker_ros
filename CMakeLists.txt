cmake_minimum_required(VERSION 3.0.2)
project(kcf_tracker_ros)

## Compile as C++11, supported in ROS Kinetic and newer
# add_compile_options(-std=c++11)


find_package(catkin REQUIRED COMPONENTS
  cv_bridge
  geometry_msgs
  image_transport
  message_generation
  roscpp
  sensor_msgs
)

find_package(Boost REQUIRED COMPONENTS
    thread
)
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

add_message_files(
  FILES
  DetectionInfo.msg
)

generate_messages(
  DEPENDENCIES
  std_msgs
)

catkin_package(
   CATKIN_DEPENDS message_runtime std_msgs
)

include_directories(
  src/kcf_tracker_lib
  ${catkin_INCLUDE_DIRS}
  ${Boost_INUCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIR}
)

add_library(kcftrackerlib
    src/kcf_tracker_lib/kcftracker.hpp
    src/kcf_tracker_lib/ffttools.hpp
    src/kcf_tracker_lib/fhog.hpp
    src/kcf_tracker_lib/labdata.hpp
    src/kcf_tracker_lib/recttools.hpp
    src/kcf_tracker_lib/tracker.h
    src/kcf_tracker_lib/kcftracker.cpp
    src/kcf_tracker_lib/fhog.cpp
)
target_link_libraries(kcftrackerlib ${catkin_LIBRARIES} ${Boost_LIBRARIES} ${OpenCV_LIBS})

add_executable(kcf_tracker
    src/kcf_tracker.cpp
)
target_link_libraries(kcf_tracker ${catkin_LIBRARIES} ${Boost_LIBRARIES} ${OpenCV_LIBS} kcftrackerlib yaml-cpp)