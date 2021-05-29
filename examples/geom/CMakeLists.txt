cmake_minimum_required(VERSION 3.20)
project(geom-example)

include(unplusplus)

add_library(geom geom.cpp)
add_unplusplus_clib(geom.hpp geom)
add_executable(geom_test geom_main.c)
target_compile_options(geom_test PUBLIC -Werror-implicit-function-declaration)
target_link_libraries(geom_test geom geom-clib)
set_target_properties(geom_test PROPERTIES LINKER_LANGUAGE CXX)
