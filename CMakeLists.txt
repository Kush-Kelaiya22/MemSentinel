cmake_minimum_required(VERSION 3.16)
project(Safe-C
    VERSION 1.2.2
    DESCRIPTION "Safe-C: Native GCC/G++ Memory Safety Plugin and Toolchain Addon with Compile-Time Enforcement and Runtime Guardrails"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

# Detect GCC Install Path for Specs Integration
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-search-dirs
    OUTPUT_VARIABLE GCC_SEARCH_DIRS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
string(REGEX MATCH "install: ([^\r\n]+)" _MATCH "${GCC_SEARCH_DIRS}")
set(GCC_INSTALL_DIR "${CMAKE_MATCH_1}")

if(NOT WIN32 AND EXISTS "${GCC_PLUGIN_DIR}/include/gcc-plugin.h")
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

# Default Toolchain Integration Targets
if(GCC_INSTALL_DIR)
    add_custom_target(enable-default
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GCC_INSTALL_DIR}/plugin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:safe_c> "${GCC_INSTALL_DIR}/plugin/$<TARGET_FILE_NAME:safe_c>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/src/specs/safe_c.specs" "${GCC_INSTALL_DIR}/specs"
        DEPENDS safe_c
        COMMENT "Enabling Safe-C by default for host GCC (${GCC_INSTALL_DIR})"
    )

    add_custom_target(disable-default
        COMMAND ${CMAKE_COMMAND} -E rm -f "${GCC_INSTALL_DIR}/specs"
        COMMENT "Disabling Safe-C default auto-loading from host GCC"
    )
endif()

# Packaging and Installation
install(TARGETS safe_c_core safe_c_rt safe_c_rt_static
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS safe_c
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/safe_c DESTINATION include)
install(FILES src/specs/safe_c.specs DESTINATION specs)
install(DIRECTORY tools DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)

set(CPACK_PACKAGE_NAME "Safe-C")
set(CPACK_PACKAGE_VENDOR "Safe-C Contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Safe-C: Native Compile-Time Memory Safety Extension for GCC and G++")
set(CPACK_PACKAGE_VERSION "1.2.2")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
