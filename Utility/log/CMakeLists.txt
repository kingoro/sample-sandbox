cmake_minimum_required(VERSION 3.20)
project(utility_log_tests C)
include(CTest)

add_executable(
    utility_log_tests
    src/utility_logger.c
    src/utility_log_console.c
    tests/test_utility_logger.c
    tests/test_utility_log_console.c
    tests/test_utility_log_main.c)

target_include_directories(utility_log_tests PRIVATE include tests)
target_compile_features(utility_log_tests PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        utility_log_tests
        PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME utility_log_unit COMMAND utility_log_tests)
