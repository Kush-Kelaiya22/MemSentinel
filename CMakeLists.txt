cmake_minimum_required(VERSION 3.16)
project(memcheck
    VERSION 1.0.0
    DESCRIPTION "High-precision compile-time memory safety checker for GCC/G++ with runtime guardrails"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler warnings and safety flags
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

# Global include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Detect GCC Plugin Development Environment
find_package(PkgConfig QUIET)
set(GCC_PLUGIN_SUPPORTED FALSE)

execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=plugin
    OUTPUT_VARIABLE GCC_PLUGIN_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(EXISTS "${GCC_PLUGIN_DIR}/include/gcc-plugin.h")
    message(STATUS "Found GCC plugin directory: ${GCC_PLUGIN_DIR}")
    set(GCC_PLUGIN_SUPPORTED TRUE)
    set(GCC_PLUGIN_INCLUDE_DIR "${GCC_PLUGIN_DIR}/include")
else()
    message(STATUS "GCC plugin development headers not detected at '${GCC_PLUGIN_DIR}'. "
                   "Native plugin will be compiled if headers are provided, "
                   "and driver-level static analyzer will be built.")
endif()

# Build components
add_subdirectory(src/core)
add_subdirectory(src/runtime)
add_subdirectory(src/driver)

if(GCC_PLUGIN_SUPPORTED)
    add_subdirectory(src/plugin)
endif()

# Testing
enable_testing()
add_subdirectory(tests)
