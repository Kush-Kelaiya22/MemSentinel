add_library(memcheck_core STATIC
    pointer_state.cpp
    analyzer.cpp
)

target_include_directories(memcheck_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(memcheck_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
