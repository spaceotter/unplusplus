cmake_minimum_required(VERSION 3.17)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/c2ffi/CMake")
set(BUILD_CONFIG ${CMAKE_BUILD_TYPE})
include(setup)

project(cifgen)

add_subdirectory(c2ffi)

if (NOT ECL_INCLUDE_DIR)
    set(ECL_INCLUDE_DIR /usr/include)
    set(ECL_LINK_DIR /usr/lib)
    set(ECL_BINARY_DIR /usr/bin)
endif()
