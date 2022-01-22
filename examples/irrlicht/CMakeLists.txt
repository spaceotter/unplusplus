cmake_minimum_required(VERSION 3.20)
project(irrlicht-example)

include(unplusplus)

#add_library(irrtest irr.cpp)
add_unplusplus_clib(Irrlicht-clib
    HEADER "${IRRLICHT_HEADER}"
    LIBRARY "${IRRLICHT}"
    EXCLUDES_FILE "${CMAKE_CURRENT_SOURCE_DIR}/excludes.txt"
    NO_DEPRECATED
    CXXFLAGS -std=gnu++17)
set_target_properties(Irrlicht-clib PROPERTIES CXX_STANDARD 17)
add_executable(irr_test irr_main.c)
target_compile_options(irr_test PUBLIC -Werror-implicit-function-declaration)
target_link_libraries(irr_test "${IRRLICHT}" Irrlicht-clib)
set_target_properties(irr_test PROPERTIES LINKER_LANGUAGE CXX)
