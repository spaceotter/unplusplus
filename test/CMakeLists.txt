project(cifgen-test)

add_library(test STATIC test.cc)
add_specgen(test libtest.hh c++ "${CMAKE_CURRENT_BINARY_DIR}")

add_library(test-cif STATIC test-cif.cc)
target_link_libraries(test-cif test)
add_specgen(test-cif test-cif.h c "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_target(test-unplusplus ALL COMMAND "$<TARGET_FILE:unplusplus>" "${libtest_SPEC_FILE}" DEPENDS libtest-spec)
