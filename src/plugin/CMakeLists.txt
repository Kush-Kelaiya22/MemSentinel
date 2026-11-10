add_library(memcheck SHARED
    memcheck_plugin.cpp
    gimple_pass.cpp
)

target_link_libraries(memcheck PRIVATE
    memcheck_core
)

target_include_directories(memcheck PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
)

if(GCC_PLUGIN_FOUND)
    target_compile_definitions(memcheck PRIVATE HAVE_GCC_PLUGIN_HEADERS)
    target_include_directories(memcheck SYSTEM PRIVATE ${GCC_PLUGIN_INCLUDE_DIR})
endif()

set_target_properties(memcheck PROPERTIES
    PREFIX ""
    OUTPUT_NAME "memcheck"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    # Enable dynamic lookup / auto-import for MinGW GCC plugin DLL
    target_link_options(memcheck PRIVATE
        "-Wl,--enable-auto-import"
        "-Wl,--export-all-symbols"
    )
endif()
