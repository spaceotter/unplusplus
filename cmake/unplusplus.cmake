cmake_minimum_required(VERSION 3.20)
include_guard(GLOBAL)

if(NOT TARGET unplusplus AND NOT EXISTS "${UNPLUSPLUS_EXECUTABLE}")
    message(FATAL_ERROR "Could not extrapolate the location of the unplusplus executable.")
endif()

function(add_unplusplus_clib name)
    # upp_clib_HEADER cxx_library
    cmake_parse_arguments(PARSE_ARGV 1 upp_clib
        "NO_DEPRECATED"
        "HEADER;LIBRARY;EXCLUDES_FILE"
        "CXXFLAGS")
    cmake_path(ABSOLUTE_PATH upp_clib_HEADER NORMALIZE)

    if(NOT DEFINED upp_clib_HEADER)
        message(FATAL_ERROR "No header supplied for ${name}")
    endif()
    if(NOT DEFINED upp_clib_LIBRARY)
        message(FATAL_ERROR "No library supplied for ${name}")
    endif()

    message(STATUS "Building C Wrapper: ${name} for ${upp_clib_LIBRARY}")

    set(upp_args "--extra-arg-before=-xc++-header")
    if(DEFINED upp_clib_EXCLUDES_FILE)
        cmake_path(ABSOLUTE_PATH upp_clib_EXCLUDES_FILE NORMALIZE)
        list(APPEND upp_args "--excludes-file")
        list(APPEND upp_args "${upp_clib_EXCLUDES_FILE}")
    endif()

    if(upp_clib_NO_DEPRECATED)
        list(APPEND upp_args "--no-deprecated")
    endif()

    foreach(arg ${upp_clib_CXXFLAGS})
        list(APPEND upp_args "--extra-arg-before=${arg}")
    endforeach()

    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.h"
        "${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/${name}.json"
        COMMAND "$<IF:$<TARGET_EXISTS:unplusplus>,$<TARGET_FILE:unplusplus>,${UNPLUSPLUS_EXECUTABLE}>"
        -o "${name}" "${upp_clib_HEADER}" ${upp_args}
        MAIN_DEPENDENCY "${upp_clib_HEADER}"
        DEPENDS unplusplus "${upp_clib_EXCLUDES_FILE}")
    add_library("${name}" "${CMAKE_CURRENT_BINARY_DIR}/${name}.cpp")
    target_include_directories("${name}" PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_options("${name}" PUBLIC "${upp_clib_CXXFLAGS}")
    target_link_libraries("${name}" "${upp_clib_LIBRARY}")

    set(${name}_HEADERS "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
    set(${name}_JSON "${CMAKE_CURRENT_BINARY_DIR}/${name}.json" PARENT_SCOPE)
endfunction()
