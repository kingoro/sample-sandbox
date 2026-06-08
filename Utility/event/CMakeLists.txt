cmake_minimum_required(VERSION 3.20)
project(utility_event_tests C)
include(CTest)

add_executable(
    utility_event_tests
    src/utility_event_queue.c
    src/utility_event_dispatcher.c
    src/utility_event_state_machine.c
    src/utility_event_trace.c
    src/utility_event_trace_log.c
    ../log/src/utility_logger.c
    ../log/src/utility_log_console.c
    tests/test_utility_event_queue.c
    tests/test_utility_event_dispatcher.c
    tests/test_utility_event_state_machine.c
    tests/test_utility_event_trace.c
    tests/test_utility_event_main.c)

target_include_directories(
    utility_event_tests
    PRIVATE include ../log/include tests)
target_compile_features(utility_event_tests PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        utility_event_tests
        PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME utility_event_unit COMMAND utility_event_tests)

add_executable(
    utility_event_integration_tests
    src/utility_event_queue.c
    src/utility_event_dispatcher.c
    src/utility_event_state_machine.c
    src/utility_event_trace.c
    src/utility_event_trace_log.c
    ../log/src/utility_logger.c
    ../log/src/utility_log_console.c
    tests/test_utility_event_integration.c)

target_include_directories(
    utility_event_integration_tests
    PRIVATE include ../log/include tests)
target_compile_features(utility_event_integration_tests PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        utility_event_integration_tests
        PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(
    NAME utility_event_integration
    COMMAND utility_event_integration_tests)
