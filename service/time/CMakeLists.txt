cmake_minimum_required(VERSION 3.20)
project(time_service_tests C)
include(CTest)
add_executable(time_service_tests
    src/time_service.c
    ../../foundation/time/src/utility_time.c
    tests/test_time_service.c)
target_include_directories(time_service_tests PRIVATE
    include
    ../../foundation/time/include)
target_compile_features(time_service_tests PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(time_service_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME time_service_unit COMMAND time_service_tests)
