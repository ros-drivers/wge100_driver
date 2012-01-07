cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
rosbuild_init()

link_directories(${PROJECT_SOURCE_DIR}/lib)
link_libraries(PvAPI)
add_definitions(-D_LINUX -D_x86)
set(EXAMPLES build/Prosilica_GigE_SDK/examples)

rosbuild_add_executable(bin/CLIpConfig ${EXAMPLES}/CLIpConfig/CLIpConfig.cpp)
rosbuild_add_executable(bin/Ping ${EXAMPLES}/Ping/Ping.cpp)
rosbuild_add_executable(bin/ListCameras ${EXAMPLES}/ListCameras/ListCameras.cpp)
rosbuild_add_executable(bin/ResetCamera ${EXAMPLES}/ResetCamera/ResetCamera.cpp)
rosbuild_add_executable(bin/ListAttributes ${EXAMPLES}/ListAttributes/ListAttributes.cpp)
