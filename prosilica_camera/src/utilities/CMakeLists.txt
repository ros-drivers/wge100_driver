# Disabled until it's mature
#rosbuild_add_executable(find_camera find_camera.cpp)

rosbuild_add_executable(write_memory write_memory.cpp)
target_link_libraries(write_memory prosilica)

rosbuild_add_executable(read_memory read_memory.cpp)
target_link_libraries(read_memory prosilica)

rosbuild_add_executable(set_ip set_ip.cpp)
target_link_libraries(set_ip prosilica)

rosbuild_add_executable(set_inhibition set_inhibition.cpp)
target_link_libraries(set_inhibition prosilica)
