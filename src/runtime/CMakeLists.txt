set(RUNTIME_SOURCES
    rt_main.cpp
    ledger.cpp
    logger.cpp
    stacktrace.cpp
)

# Shared Runtime Library
add_library(safe_c_rt SHARED
    ${RUNTIME_SOURCES}
)

add_library(safe_cpp_rt ALIAS safe_c_rt)
add_library(memsentinel_rt ALIAS safe_c_rt)

target_include_directories(safe_c_rt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(safe_c_rt PRIVATE SAFE_C_BUILD_DLL SAFE_CPP_BUILD_DLL MEMCHECK_BUILD_DLL)

set_target_properties(safe_c_rt PROPERTIES
    OUTPUT_NAME "safe_c_rt"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_libraries(safe_c_rt PRIVATE dbghelp)
endif()

# Static Runtime Library
add_library(safe_c_rt_static STATIC
    ${RUNTIME_SOURCES}
)

add_library(safe_cpp_rt_static ALIAS safe_c_rt_static)
add_library(memsentinel_rt_static ALIAS safe_c_rt_static)

target_include_directories(safe_c_rt_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(safe_c_rt_static PROPERTIES
    OUTPUT_NAME "safe_c_rt"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_libraries(safe_c_rt_static PRIVATE dbghelp)
endif()
