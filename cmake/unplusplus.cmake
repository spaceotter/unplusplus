if (NOT add_unplusplus_clib)
    function(add_unplusplus_clib header_file cxx_library)
        cmake_path(ABSOLUTE_PATH header_file NORMALIZE)
        set(clib_name "${cxx_library}-clib")
        message("clib name ${clib_name}")

        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${cxx_library}-clib.h"
            "${CMAKE_CURRENT_BINARY_DIR}/${cxx_library}-clib.cpp"
            COMMAND "${UNPLUSPLUS_EXECUTABLE}"
            -o "${cxx_library}-clib" "${header_file}" "--extra-arg-before=-xc++-header"
            MAIN_DEPENDENCY "${header_file}"
            DEPENDS unplusplus)
        add_library("${clib_name}" "${CMAKE_CURRENT_BINARY_DIR}/${cxx_library}-clib.cpp")
        target_include_directories("${clib_name}" PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries("${clib_name}" "${cxx_library}")
    endfunction()
endif()
