add_executable(list main.cc colors.hh colors.cc dirent.hh dirent.cc)
target_compile_options(list PRIVATE ${ADDITIONAL_WALL_FLAGS})
target_link_libraries(list PRIVATE arch)
