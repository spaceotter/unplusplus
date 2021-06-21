if (NOT add_unplusplus_clib)
    function(add_unplusplus_clib header_file cxx_library)
        cmake_path(ABSOLUTE_PATH header_file NORMALIZE)
        get_filename_component(clib_name "${cxx_library}" NAME_WE)
        if (clib_name MATCHES "^lib.*")
            string(SUBSTRING "${clib_name}" 3 -1 clib_name)
        endif()
        set(clib_name "${clib_name}-clib")
        message("clib name ${clib_name}")

        set(excludes_file_name "")
        foreach (arg ${ARGN})
            if (lastarg STREQUAL "--excludes-file")
                set(excludes_file_name "${arg}")
            endif()
            set(lastarg "${arg}")
        endforeach()

        if(excludes_file_name)
            cmake_path(ABSOLUTE_PATH excludes_file_name NORMALIZE)
            message("excludes file: ${excludes_file_name}")
        endif()

        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${clib_name}.h"
            "${CMAKE_CURRENT_BINARY_DIR}/${clib_name}.cpp"
            COMMAND "${UNPLUSPLUS_EXECUTABLE}"
            -o "${clib_name}" "${header_file}" "--extra-arg-before=-xc++-header" ${ARGN}
            MAIN_DEPENDENCY "${header_file}"
            DEPENDS unplusplus "${excludes_file_name}")
        add_library("${clib_name}" "${CMAKE_CURRENT_BINARY_DIR}/${clib_name}.cpp")
        target_include_directories("${clib_name}" PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries("${clib_name}" "${cxx_library}")
    endfunction()
endif()
