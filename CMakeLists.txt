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

function(add_specgen header_file lang dir)
    file(REAL_PATH "${header_file}" header_file BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(base "${header_file}" NAME_WLE)
    add_custom_command(OUTPUT "${dir}/${base}.spec"
        COMMAND
        "env"
        "PATH=$<TARGET_PROPERTY:c2ffi,RUNTIME_OUTPUT_DIRECTORY>:$$PATH"
        "${CMAKE_CURRENT_SOURCE_DIR}/specgen"
        "${header_file}"
        "${lang}"
        DEPENDS "${header_file}" c2ffi
        WORKING_DIRECTORY "${dir}")
    add_custom_target("${base}-spec" DEPENDS "${dir}/${base}.spec")
endfunction()

add_library(test STATIC test.cc)
add_specgen(libtest.hh c++ "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test libtest-spec)

add_library(test-cif STATIC test-cif.cc)
target_link_libraries(test-cif test)
add_specgen(test-cif.h c "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test-cif test-cif-spec)
