cmake_minimum_required(VERSION 3.20)
project(utility_event_tests C)
include(CTest)

add_executable(
    utility_event_tests
    src/utility_event_queue.c
    src/utility_event_dispatcher.c
    tests/test_utility_event_queue.c
    tests/test_utility_event_dispatcher.c
    tests/test_utility_event_main.c)

target_include_directories(utility_event_tests PRIVATE include tests)
target_compile_features(utility_event_tests PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        utility_event_tests
        PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME utility_event_unit COMMAND utility_event_tests)
