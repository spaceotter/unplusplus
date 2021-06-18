cmake_minimum_required(VERSION 3.20)
project(unplusplus)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(LLVM 12.0.0 CONFIG)
if(NOT LLVM_FOUND)
    find_package(LLVM 11.1.0 CONFIG)
endif()
if(NOT LLVM_FOUND)
    find_package(LLVM 11.0.0 REQUIRED CONFIG)
endif()

# Find Clang resource directory with Clang executable.
if(NOT CLANG_RESOURCE_DIR)
    set(CLANG_EXECUTABLE "clang")
    if(NOT CLANG_EXECUTABLE)
        message(FATAL_ERROR "clang executable not found.")
    endif()

    execute_process(
        COMMAND ${CLANG_EXECUTABLE} -print-resource-dir
        RESULT_VARIABLE CLANG_FIND_RESOURCE_DIR_RESULT
        OUTPUT_VARIABLE CLANG_RESOURCE_DIR
        ERROR_VARIABLE CLANG_FIND_RESOURCE_DIR_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

    if(CLANG_FIND_RESOURCE_DIR_RESULT)
        message(FATAL_ERROR "Error retrieving Clang resource directory with Clang \
            executable. Output:\n ${CLANG_FIND_RESOURCE_DIR_ERROR}")
    endif()
endif()

set(SOURCE_FILES
    src/main.cpp
    src/identifier.cpp
    src/outputs.cpp
    src/action.cpp
    src/filter.cpp
    src/cxxrecord.cpp
    src/function.cpp
    src/enum.cpp
    src/jobs.cpp
    src/options.cpp)

add_executable(unplusplus ${SOURCE_FILES})
target_compile_definitions(unplusplus PUBLIC "CLANG_RESOURCE_DIRECTORY=R\"\(${CLANG_RESOURCE_DIR}\)\"")
set_target_properties(unplusplus PROPERTIES CXX_STANDARD 17)
target_include_directories(unplusplus PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# For development, it's recommended to use clang/LLVM which include the debugging symbols and assertions
# target_include_directories(unplusplus PUBLIC ~/src/clang11/src/clang-11.0.1.src/include)
# target_include_directories(unplusplus PUBLIC ~/src/llvm11/src/llvm-11.0.1.src/include)
# target_include_directories(unplusplus PUBLIC ~/src/llvm11/src/llvm-11.0.1.src/build/include)
# target_link_directories(unplusplus PUBLIC ~/src/clang11/src/clang-11.0.1.src/build/lib)
# target_link_directories(unplusplus PUBLIC ~/src/llvm11/src/llvm-11.0.1.src/build/lib)
# target_compile_definitions(unplusplus PUBLIC _GNU_SOURCE __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __STDC_LIMIT_MACROS _DEBUG)
# target_link_libraries(unplusplus PUBLIC ~/src/clang11/src/clang-11.0.1.src/build/lib/libclang-cpp.so ~/src/llvm11/src/llvm-11.0.1.src/build/lib/libLLVM-11.so)

target_link_libraries(unplusplus PUBLIC clang-cpp LLVM)

set(APP_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set_target_properties(unplusplus PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${APP_BIN_DIR}"
    )

set(UNPLUSPLUS_EXECUTABLE "${APP_BIN_DIR}/unplusplus")
add_subdirectory(examples)

install(TARGETS unplusplus DESTINATION bin)
