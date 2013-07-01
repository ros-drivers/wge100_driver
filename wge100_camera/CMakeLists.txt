cmake_minimum_required(VERSION 2.8.3)
project(wge100_camera)

find_package(catkin REQUIRED 
   roscpp
   rospy
   diagnostic_updater 
   driver_base 
   self_test 
   dynamic_reconfigure 
   camera_calibration_parsers 
   timestamp_tools 
   image_transport 
   std_msgs 
   sensor_msgs 
   message_generation
)

find_package(Boost REQUIRED)

include_directories(include ${catkin_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})

# generating dynamic configure parameters
# TODO: add dependency to node that actual uses this
generate_dynamic_reconfigure_options(
  cfg/WGE100Camera.cfg 
  cfg/WGE100MultiConfigurator.cfg
)

# service generation
add_service_files(
  DIRECTORY srv
  FILES
  BoardConfig.srv
)

generate_messages(
  DEPENDENCIES
  std_msgs
  sensor_msgs
)

# package definition
catkin_package(
  CATKIN_DEPENDS message_runtime
)

# OLD ROSBUILD-STUFF, TODO: STILL TO BE PORTED TO CATKIN
#add_rostest(test/test_with_sim_hztest.xml)
#add_rostest(test/test_with_sim_selftest.xml)
# add_subdirectory(src)
# add_subdirectory(test)

# TODO: INSTALL?

# TODO: python scripts?
