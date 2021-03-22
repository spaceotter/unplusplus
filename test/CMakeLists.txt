project(cifgen-test)

add_library(test STATIC test.cc)
add_specgen(test libtest.hh c++ "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libtest.h" "${CMAKE_CURRENT_BINARY_DIR}/libtest.cc"
    COMMAND "$<TARGET_FILE:unplusplus>" "${libtest_SPEC_FILE}"
    DEPENDS "${libtest_SPEC_FILE}" unplusplus
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_library(test-upp STATIC "${CMAKE_CURRENT_BINARY_DIR}/libtest.cc")
target_include_directories(test-upp PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(test-upp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(test-upp test)

add_executable(test-upp-main main.c)
target_link_libraries(test-upp-main test-upp)
