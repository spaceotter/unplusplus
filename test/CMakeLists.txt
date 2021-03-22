project(cifgen-test)

add_library(test STATIC test.cc)
add_specgen(libtest.hh c++ "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test libtest-spec)

add_library(test-cif STATIC test-cif.cc)
target_link_libraries(test-cif test)
add_specgen(test-cif.h c "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(test-cif test-cif-spec)

add_custom_target(test-cifgen ALL COMMAND "$<TARGET_FILE:cifgen>" "${libtest_SPEC_FILE}" DEPENDS libtest-spec)
