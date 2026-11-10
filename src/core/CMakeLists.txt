add_library(safe_c_core STATIC
    pointer_state.cpp
    analyzer.cpp
)

add_library(safe_cpp_core ALIAS safe_c_core)
add_library(memcheck_core ALIAS safe_c_core)

target_include_directories(safe_c_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(safe_c_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
