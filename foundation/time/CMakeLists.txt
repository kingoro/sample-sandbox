cmake_minimum_required(VERSION 3.20)
project(utility_time_tests C)
include(CTest)
add_executable(utility_time_tests
    src/utility_time.c
    src/utility_time_rfc3339.c
    tests/test_utility_time.c)
target_include_directories(utility_time_tests PRIVATE include)
target_compile_features(utility_time_tests PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(utility_time_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME utility_time_unit COMMAND utility_time_tests)
