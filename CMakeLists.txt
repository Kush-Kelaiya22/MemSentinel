cmake_minimum_required(VERSION 3.16)
project(memcheck
    VERSION 1.0.0
    DESCRIPTION "Native GCC/G++ Memory Safety Plugin with Compile-Time Enforcement and Runtime Guardrails"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
    )
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Detect GCC Plugin Development Environment
execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=plugin
    OUTPUT_VARIABLE GCC_PLUGIN_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(EXISTS "${GCC_PLUGIN_DIR}/include/gcc-plugin.h")
    message(STATUS "Found GCC plugin headers at: ${GCC_PLUGIN_DIR}")
    set(GCC_PLUGIN_INCLUDE_DIR "${GCC_PLUGIN_DIR}/include")
    set(GCC_PLUGIN_FOUND TRUE)
else()
    message(STATUS "Note: Host compiler reported plugin path '${GCC_PLUGIN_DIR}'.")
    set(GCC_PLUGIN_FOUND FALSE)
endif()

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/plugin)
add_subdirectory(src/runtime)
add_subdirectory(tests)
