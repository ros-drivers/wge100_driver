cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)

set(ROS_BUILD_TYPE RelWithDebInfo)
rosbuild_init()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

rosbuild_find_ros_package(dynamic_reconfigure)
include(${dynamic_reconfigure_PACKAGE_PATH}/cmake/cfgbuild.cmake)

rosbuild_add_boost_directories()

rosbuild_add_rostest(test/test_with_sim_hztest.xml)
rosbuild_add_rostest(test/test_with_sim_selftest.xml)

#FIXME Remove next line at some point
#rosbuild_remove_compile_flags(wge100_camera -W) 

gencfg()
rosbuild_gensrv()

add_subdirectory(src)
add_subdirectory(test)
