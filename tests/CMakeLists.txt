add_executable(safe_cpp_tests
    test_analyzer.cpp
)

target_link_libraries(safe_cpp_tests PRIVATE
    safe_cpp_core
    safe_cpp_rt_static
)

target_include_directories(safe_cpp_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/runtime
)

target_compile_definitions(safe_cpp_tests PRIVATE
    TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

set_target_properties(safe_cpp_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME safe_cpp_unit_tests COMMAND safe_cpp_tests)
