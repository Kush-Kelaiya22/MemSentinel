add_library(safe_cpp_core STATIC
    pointer_state.cpp
    analyzer.cpp
)

add_library(memcheck_core ALIAS safe_cpp_core)

target_include_directories(safe_cpp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(safe_cpp_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
