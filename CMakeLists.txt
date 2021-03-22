cmake_minimum_required(VERSION 3.17)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/c2ffi/CMake")
set(BUILD_CONFIG ${CMAKE_BUILD_TYPE})
include(setup)

project(unplusplus)

add_subdirectory(c2ffi)

include(FindPkgConfig)
pkg_check_modules(jsoncpp REQUIRED IMPORTED_TARGET jsoncpp)

if (NOT ECL_INCLUDE_DIR)
    set(ECL_INCLUDE_DIR /usr/include)
    set(ECL_LINK_DIR /usr/lib)
    set(ECL_BINARY_DIR /usr/bin)
endif()

set(SPECGEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/specgen")
function(add_specgen src_target header_file lang dir)
    file(REAL_PATH "${header_file}" header_file BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(base "${header_file}" NAME_WLE)
    set("${base}_SPEC_FILE" "${dir}/${base}.spec" PARENT_SCOPE)
    add_custom_command(OUTPUT "${dir}/${base}.spec"
        COMMAND
        "env"
        "PATH=$<TARGET_PROPERTY:c2ffi,RUNTIME_OUTPUT_DIRECTORY>:$$PATH"
        "${SPECGEN_SCRIPT}"
        "${header_file}"
        "${lang}"
        DEPENDS "${header_file}" c2ffi "${src_target}" "${SPECGEN_SCRIPT}"
        WORKING_DIRECTORY "${dir}")
    add_custom_target("${base}-spec" DEPENDS "${dir}/${base}.spec")
endfunction()

add_executable(unplusplus src/main.cc)
target_link_libraries(unplusplus PkgConfig::jsoncpp)
target_cxx_std(unplusplus 17)

add_subdirectory(test)
