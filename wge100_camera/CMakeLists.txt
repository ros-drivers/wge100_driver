cmake_minimum_required(VERSION 2.8.3)
project(wge100_camera)

find_package(catkin REQUIRED 
   roscpp
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
   rostest
)

find_package(Boost REQUIRED COMPONENTS thread)

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
  CATKIN_DEPENDS message_runtime dynamic_reconfigure
)

# wge100lib
add_library(wge100camera
   src/wge100lib/host_netutil.c
   src/wge100lib/list.c 
   src/wge100lib/wge100lib.c
   src/wge100lib/mt9v.cpp)
target_link_libraries(wge100camera ${catkin_LIBRARIES}) # ros/console

# Nodes
add_executable(wge100_camera_node src/nodes/wge100_camera_node.cpp)
target_link_libraries(wge100_camera_node 
   wge100camera 
   ${catkin_LIBRARIES} 
   ${boost_LIBRARIES})
add_dependencies(wge100_camera_node 
   ${catkin_EXPORTED_TARGETS} 
   ${wge100_camera_EXPORTED_TARGETS})

# Utilities
add_executable(access_register src/utilities/access_register.cpp)
target_link_libraries(access_register wge100camera)

add_executable(set_calibration src/utilities/set_calibration.cpp)
target_link_libraries(set_calibration wge100camera)

add_executable(set_name src/utilities/set_name.cpp)
target_link_libraries(set_name wge100camera)

add_executable(reconfigure_cam src/utilities/reconfigure_cam.cpp)
target_link_libraries(reconfigure_cam wge100camera)

add_executable(reset_cam src/utilities/reset_cam.cpp)
target_link_libraries(reset_cam wge100camera)

add_executable(upload_mcs src/utilities/upload_mcs.cpp)
target_link_libraries(upload_mcs wge100camera)

add_executable(discover src/utilities/discover.cpp)
target_link_libraries(discover wge100camera)

add_executable(check_flash src/utilities/check_flash.cpp)
target_link_libraries(check_flash wge100camera)

add_executable(read_all_flash src/utilities/read_all_flash.cpp)
target_link_libraries(read_all_flash wge100camera)

# Tests
add_executable(wge100_sim test/wge100_sim.cc)
target_link_libraries(wge100_sim wge100camera)

add_executable(stress_discover test/cycle_tests/stress_discover.cpp)
target_link_libraries(stress_discover wge100camera)

add_rostest(test/test_with_sim_hztest.xml)
add_rostest(test/test_with_sim_selftest.xml)

# INSTALL
install(TARGETS 
   wge100_camera_node 
   access_register 
   set_calibration 
   set_name 
   reconfigure_cam
   reset_cam
   upload_mcs
   discover
   check_flash
   read_all_flash
   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})

install(PROGRAMS nodes/wge100_multi_configurator
   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})

install(TARGETS wge100camera
   RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION}
   ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
   LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION})

install(DIRECTORY include/${PROJECT_NAME}/
   DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION})

install(FILES wge100_camera_demo.launch
   DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION})

install(DIRECTORY firmware_images
   DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION})
