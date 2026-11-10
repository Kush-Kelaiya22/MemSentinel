add_executable(memcheck_tests
    test_analyzer.cpp
)

target_link_libraries(memcheck_tests PRIVATE
    memcheck_core
    memsentinel_rt_static
)

target_include_directories(memcheck_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/runtime
)

target_compile_definitions(memcheck_tests PRIVATE
    TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

set_target_properties(memcheck_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME memcheck_unit_tests COMMAND memcheck_tests)
