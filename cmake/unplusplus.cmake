if (NOT add_unplusplus_clib)
    function(add_unplusplus_clib header_file cxx_library)
        cmake_path(ABSOLUTE_PATH header_file NORMALIZE)
        get_filename_component(clib_name "${cxx_library}" NAME_WE)
        set(clib_name "${clib_name}-clib")
        message("clib name ${clib_name}")

        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${clib_name}.h"
            "${CMAKE_CURRENT_BINARY_DIR}/${clib_name}.cpp"
            COMMAND "${UNPLUSPLUS_EXECUTABLE}"
            -o "${clib_name}" "${header_file}" "--extra-arg-before=-xc++-header" ${ARGN}
            MAIN_DEPENDENCY "${header_file}"
            DEPENDS unplusplus)
        add_library("${clib_name}" "${CMAKE_CURRENT_BINARY_DIR}/${clib_name}.cpp")
        target_include_directories("${clib_name}" PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries("${clib_name}" "${cxx_library}")
    endfunction()
endif()
