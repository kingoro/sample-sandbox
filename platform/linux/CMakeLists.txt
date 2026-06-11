cmake_minimum_required(VERSION 3.20)
project(platform_linux_time_tests C)
include(CTest)
add_executable(platform_linux_time_tests
    src/platform_linux_time.c
    ../../foundation/time/src/utility_time.c
    tests/test_platform_linux_time.c)
target_include_directories(platform_linux_time_tests PRIVATE
    include
    src
    ../../foundation/time/include)
target_compile_features(platform_linux_time_tests PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(platform_linux_time_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME platform_linux_time_unit COMMAND platform_linux_time_tests)
