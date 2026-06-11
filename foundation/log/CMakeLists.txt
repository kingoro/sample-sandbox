cmake_minimum_required(VERSION 3.20)
project(utility_log_tests C)
include(CTest)
find_package(Threads REQUIRED)

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

function(add_utility_log_example target source)
    add_executable(
        ${target}
        src/utility_logger.c
        src/utility_log_console.c
        ${source})
    target_include_directories(${target} PRIVATE include)
    target_compile_features(${target} PRIVATE c_std_11)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            ${target}
            PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_utility_log_example(
    utility_log_example_basic
    examples/basic_default_logger.c)
add_utility_log_example(
    utility_log_example_ring
    examples/ring_maintenance.c)
add_utility_log_example(
    utility_log_example_threads
    examples/thread_safe_logger.c)
target_link_libraries(
    utility_log_example_threads
    PRIVATE Threads::Threads)
