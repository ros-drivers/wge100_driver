rosbuild_add_executable(wge100_sim wge100_sim.cc)
target_link_libraries(wge100_sim wge100camera)

add_subdirectory(cycle_tests)
