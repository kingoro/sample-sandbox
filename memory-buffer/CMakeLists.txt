cmake_minimum_required(VERSION 3.20)
project(memory_buffer_c_example C)
include(CTest)

set(MB_RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo-target")

add_custom_command(
    OUTPUT "${MB_RUST_TARGET_DIR}/release/libmemory_buffer.a"
    COMMAND
        cargo build
        --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        --release
        --no-default-features
        --target-dir "${MB_RUST_TARGET_DIR}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lib.rs"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/buffer.rs"
    VERBATIM)

add_custom_target(memory_buffer_rust ALL
    DEPENDS "${MB_RUST_TARGET_DIR}/release/libmemory_buffer.a")

add_library(memory_buffer STATIC IMPORTED GLOBAL)
set_target_properties(memory_buffer PROPERTIES
    IMPORTED_LOCATION "${MB_RUST_TARGET_DIR}/release/libmemory_buffer.a")
add_dependencies(memory_buffer memory_buffer_rust)

add_executable(memory_buffer_c_example examples/c_usage.c)
target_include_directories(memory_buffer_c_example PRIVATE include)
target_link_libraries(memory_buffer_c_example PRIVATE memory_buffer pthread dl m)

add_executable(
    memory_buffer_event_integration
    tests/test_event_buffer_integration.c
    src/memory_buffer_event_adapter.c
    ../foundation/event/src/utility_event_buffer.c
    ../foundation/event/src/utility_event_dispatcher.c)
target_include_directories(
    memory_buffer_event_integration
    PRIVATE
        include
        ../foundation/event/include)
target_compile_features(memory_buffer_event_integration PRIVATE c_std_11)
target_link_libraries(
    memory_buffer_event_integration
    PRIVATE memory_buffer pthread dl m)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        memory_buffer_event_integration
        PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME memory_buffer_c_integration COMMAND memory_buffer_c_example)
add_test(
    NAME memory_buffer_event_integration
    COMMAND memory_buffer_event_integration)
