add_library(memsentinel_plugin SHARED
    memcheck_plugin.cpp
    gimple_pass.cpp
)

target_link_libraries(memsentinel_plugin PRIVATE
    memcheck_core
)

target_include_directories(memsentinel_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
)

if(GCC_PLUGIN_FOUND)
    target_compile_definitions(memsentinel_plugin PRIVATE HAVE_GCC_PLUGIN_HEADERS)
    target_include_directories(memsentinel_plugin SYSTEM PRIVATE ${GCC_PLUGIN_INCLUDE_DIR})
endif()

set_target_properties(memsentinel_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "memsentinel"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_options(memsentinel_plugin PRIVATE
        "-Wl,--enable-auto-import"
        "-Wl,--export-all-symbols"
    )
endif()
