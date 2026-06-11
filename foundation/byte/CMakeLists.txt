cmake_minimum_required(VERSION 3.20)
project(utility_byte_tests C)
include(CTest)
add_executable(utility_byte_tests
    src/utility_byte_reader.c src/utility_byte_writer.c
    tests/test_utility_byte.c)
target_include_directories(utility_byte_tests PRIVATE include tests)
target_compile_features(utility_byte_tests PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(utility_byte_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME utility_byte_unit COMMAND utility_byte_tests)
