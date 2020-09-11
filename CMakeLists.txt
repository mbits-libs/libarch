if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  cmake_minimum_required (VERSION 3.17)
  project (arch CXX)
  set(LIBARCH_TESTING_DEFAULT ON)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  conan_basic_setup(TARGETS)

  add_library(arch::deps INTERFACE IMPORTED)
  target_link_libraries(arch::deps INTERFACE
    CONAN_PKG::libzip
    CONAN_PKG::zlib
    CONAN_PKG::bzip2
    CONAN_PKG::lzma)

else()
  message(STATUS "Libarch: Subdir")
  set(LIBARCH_TESTING_DEFAULT OFF)
endif()

set(LIBARCH_TESTING ${LIBARCH_TESTING_DEFAULT} CACHE BOOL "Compile and/or run self-tests")


if (MSVC)
    set(ADDITIONAL_WALL_FLAGS
        /Zc:__cplusplus
        /permissive-
        /W4
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /we4289
        /w14296
        /w14311
        /w14545
        /w14546
        /w14547
        /w14549
        /w14555
        /w14619
        /w14640
        /w14826
        /w14905
        /w14906
        /w14928
        /w14946
    )
else()
    set(ADDITIONAL_WALL_FLAGS
        -Wall -Wextra
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        list(APPEND ADDITIONAL_WALL_FLAGS -fcolor-diagnostics) # -Wlifetime
    else()
        list(APPEND ADDITIONAL_WALL_FLAGS
            -fdiagnostics-color
            -Wmisleading-indentation
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
        )
    endif()
endif()

set(SRCS
    src/base/archive.cc
    src/base/decompressor.cc
    src/base/io/seekable.cc
    src/base/io/stream.cc
    src/base/io/writeable.cc
    src/base/entry.cc
    include/arch/base/archive.hh
    include/arch/base/decompressor.hh
    include/arch/base/fs.hh
    include/arch/base/io/seekable.hh
    include/arch/base/io/stream.hh
    include/arch/base/io/writeable.hh
    include/arch/base/entry.hh
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_library(arch STATIC ${SRCS})
target_compile_options(arch PRIVATE ${ADDITIONAL_WALL_FLAGS})
target_include_directories(arch
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if (TARGET arch::deps)
    target_link_libraries(arch PUBLIC arch::deps)
endif()

