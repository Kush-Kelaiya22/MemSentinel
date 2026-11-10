add_library(memsentinel SHARED
    memcheck_plugin.cpp
    gimple_pass.cpp
)

target_link_libraries(memsentinel PRIVATE
    memcheck_core
)

target_include_directories(memsentinel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
)

if(GCC_PLUGIN_FOUND)
    target_compile_definitions(memsentinel PRIVATE HAVE_GCC_PLUGIN_HEADERS)
    target_include_directories(memsentinel SYSTEM PRIVATE ${GCC_PLUGIN_INCLUDE_DIR})
endif()

set_target_properties(memsentinel PROPERTIES
    PREFIX ""
    OUTPUT_NAME "memsentinel"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_options(memsentinel PRIVATE
        "-Wl,--enable-auto-import"
        "-Wl,--export-all-symbols"
    )
endif()
