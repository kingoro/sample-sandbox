cmake_minimum_required(VERSION 3.20)
project(domain_service_tests C)
include(CTest)
find_package(Threads REQUIRED)

set(DOMAIN_COMMON_SOURCES
    domain_event_publisher.c
    domain_workflow.c
    domain_workflow_loader.c
    domain_service.c
    ../unit/unit_mock.c
    ../../foundation/event/src/utility_event_publisher.c
    ../../foundation/event/src/utility_event_queue.c
    ../../foundation/event/src/utility_event_dispatcher.c
    ../../foundation/event/src/utility_event_state_machine.c
    ../../foundation/event/src/utility_event_trace.c
    ../../foundation/log/src/utility_logger.c
    ../../foundation/log/src/utility_log_console.c)

add_executable(
    domain_workflow_tests
    ${DOMAIN_COMMON_SOURCES}
    tests/test_domain_workflow_loader.c)

target_include_directories(
    domain_workflow_tests
    PRIVATE
        .
        ../unit
        ../../foundation/event/include
        ../../foundation/log/include)
target_compile_definitions(
    domain_workflow_tests
    PRIVATE
        DOMAIN_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
target_compile_features(domain_workflow_tests PRIVATE c_std_11)
target_link_libraries(domain_workflow_tests PRIVATE Threads::Threads)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        domain_workflow_tests
        PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME domain_workflow_unit COMMAND domain_workflow_tests)
