add_library(safe_c_plugin SHARED
    safe_c_plugin.cpp
    gimple_pass.cpp
)

target_compile_definitions(safe_c_plugin PRIVATE SAFE_C_DISABLE SAFE_CPP_DISABLE)

add_library(safe_cpp_plugin ALIAS safe_c_plugin)
add_library(memsentinel_plugin ALIAS safe_c_plugin)

target_link_libraries(safe_c_plugin PRIVATE
    safe_c_core
)

target_include_directories(safe_c_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
)

if(GCC_PLUGIN_FOUND)
    target_compile_definitions(safe_c_plugin PRIVATE HAVE_GCC_PLUGIN_HEADERS)
    target_include_directories(safe_c_plugin SYSTEM PRIVATE ${GCC_PLUGIN_INCLUDE_DIR})
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(safe_c_plugin PRIVATE -Wno-format-diag -Wno-pedantic)
    endif()
endif()

set_target_properties(safe_c_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "safe_c"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_options(safe_c_plugin PRIVATE
        "-Wl,--enable-auto-import"
        "-Wl,--export-all-symbols"
    )

    # Windows Native MinGW Addon Library
    add_library(safe_c_addon SHARED
        windows_addon.cpp
    )

    target_compile_definitions(safe_c_addon PRIVATE SAFE_C_DISABLE SAFE_CPP_DISABLE)

    add_library(safe_cpp_addon ALIAS safe_c_addon)

    target_link_libraries(safe_c_addon PRIVATE
        safe_c_core
    )

    target_include_directories(safe_c_addon PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/core
    )

    set_target_properties(safe_c_addon PROPERTIES
        PREFIX ""
        OUTPUT_NAME "libwinpthread-1"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/addon"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/addon"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    target_link_options(safe_c_addon PRIVATE
        "-Wl,--enable-auto-import"
        "-Wl,--export-all-symbols"
        "-static-libgcc"
        "-static-libstdc++"
    )
endif()
