set(RUNTIME_SOURCES
    rt_main.cpp
    ledger.cpp
    logger.cpp
    stacktrace.cpp
)

# Shared Runtime Library
add_library(memcheck_rt SHARED
    ${RUNTIME_SOURCES}
)

target_include_directories(memcheck_rt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(memcheck_rt PRIVATE MEMCHECK_BUILD_DLL)

set_target_properties(memcheck_rt PROPERTIES
    OUTPUT_NAME "memcheck_rt"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_libraries(memcheck_rt PRIVATE dbghelp)
endif()

# Static Runtime Library
add_library(memcheck_rt_static STATIC
    ${RUNTIME_SOURCES}
)

target_include_directories(memcheck_rt_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(memcheck_rt_static PROPERTIES
    OUTPUT_NAME "memcheck_rt"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_libraries(memcheck_rt_static PRIVATE dbghelp)
endif()
