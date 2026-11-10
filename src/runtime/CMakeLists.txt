set(RUNTIME_SOURCES
    rt_main.cpp
    ledger.cpp
    logger.cpp
    stacktrace.cpp
)

# Shared Runtime Library
add_library(memsentinel_rt SHARED
    ${RUNTIME_SOURCES}
)

target_include_directories(memsentinel_rt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(memsentinel_rt PRIVATE MEMCHECK_BUILD_DLL)

set_target_properties(memsentinel_rt PROPERTIES
    OUTPUT_NAME "memsentinel_rt"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_libraries(memsentinel_rt PRIVATE dbghelp)
endif()

# Static Runtime Library
add_library(memsentinel_rt_static STATIC
    ${RUNTIME_SOURCES}
)

target_include_directories(memsentinel_rt_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(memsentinel_rt_static PROPERTIES
    OUTPUT_NAME "memsentinel_rt"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    target_link_libraries(memsentinel_rt_static PRIVATE dbghelp)
endif()
