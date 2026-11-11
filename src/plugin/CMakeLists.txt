set(SAFE_C_PLUGIN_SRCS
    safe_c_plugin.cpp
    gimple_pass.cpp
)

if(WIN32)
    list(APPEND SAFE_C_PLUGIN_SRCS windows_addon.cpp)
endif()

add_library(safe_c SHARED
    ${SAFE_C_PLUGIN_SRCS}
)

target_compile_definitions(safe_c PRIVATE SAFE_C_DISABLE SAFE_CPP_DISABLE)

target_link_libraries(safe_c PRIVATE
    safe_c_core
)

target_include_directories(safe_c PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
)

if(GCC_PLUGIN_FOUND)
    target_compile_definitions(safe_c PRIVATE HAVE_GCC_PLUGIN_HEADERS)
    target_include_directories(safe_c SYSTEM PRIVATE ${GCC_PLUGIN_INCLUDE_DIR})
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(safe_c PRIVATE -Wno-format-diag -Wno-pedantic -fno-rtti)
    endif()
endif()

set_target_properties(safe_c PROPERTIES
    PREFIX ""
    OUTPUT_NAME "safe_c"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_options(safe_c PRIVATE
        "-Wl,--enable-auto-import"
        "-Wl,--export-all-symbols"
        "-static-libgcc"
        "-static-libstdc++"
    )
endif()

# Aliases for backwards compatibility
add_library(safe_c_plugin ALIAS safe_c)
add_library(safe_c_addon ALIAS safe_c)
add_library(safe_cpp_plugin ALIAS safe_c)
add_library(safe_cpp_addon ALIAS safe_c)
add_library(memsentinel_plugin ALIAS safe_c)
