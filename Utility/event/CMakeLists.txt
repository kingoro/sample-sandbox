cmake_minimum_required(VERSION 3.20)
project(utility_event_tests C)
include(CTest)

add_executable(
    utility_event_tests
    src/utility_event_buffer.c
    src/utility_event_contract.c
    src/utility_event_queue.c
    src/utility_event_dispatcher.c
    src/utility_event_executor.c
    src/utility_event_metrics.c
    src/utility_event_publisher.c
    src/utility_event_state_machine.c
    src/utility_event_timer.c
    src/utility_event_trace.c
    src/utility_event_trace_log.c
    ../log/src/utility_logger.c
    ../log/src/utility_log_console.c
    tests/test_utility_event_buffer.c
    tests/test_utility_event_contract.c
    tests/test_utility_event_queue.c
    tests/test_utility_event_dispatcher.c
    tests/test_utility_event_executor.c
    tests/test_utility_event_metrics.c
    tests/test_utility_event_publisher.c
    tests/test_utility_event_state_machine.c
    tests/test_utility_event_timer.c
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
    src/utility_event_buffer.c
    src/utility_event_contract.c
    src/utility_event_queue.c
    src/utility_event_dispatcher.c
    src/utility_event_executor.c
    src/utility_event_metrics.c
    src/utility_event_publisher.c
    src/utility_event_state_machine.c
    src/utility_event_timer.c
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

function(add_utility_event_example target source)
    add_executable(
        ${target}
        src/utility_event_buffer.c
        src/utility_event_contract.c
        src/utility_event_queue.c
        src/utility_event_dispatcher.c
        src/utility_event_executor.c
        src/utility_event_metrics.c
        src/utility_event_publisher.c
        src/utility_event_state_machine.c
        src/utility_event_timer.c
        src/utility_event_trace.c
        src/utility_event_trace_log.c
        ../log/src/utility_logger.c
        ../log/src/utility_log_console.c
        ${source})
    target_include_directories(
        ${target}
        PRIVATE include ../log/include)
    target_compile_features(${target} PRIVATE c_std_11)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            ${target}
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_utility_event_example(
    utility_event_example_state_machine
    examples/state_machine_dispatch.c)
add_utility_event_example(
    utility_event_example_executor
    examples/scheduled_executor.c)
add_utility_event_example(
    utility_event_example_payload
    examples/payload_lifecycle.c)
