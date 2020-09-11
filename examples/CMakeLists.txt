add_subdirectory(list)

add_executable(expand expand.cc)
target_compile_options(expand PRIVATE ${ADDITIONAL_WALL_FLAGS})
target_link_libraries(expand PRIVATE arch)
