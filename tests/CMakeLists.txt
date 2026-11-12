add_executable(safe_c_tests
    test_analyzer.cpp
)

target_link_libraries(safe_c_tests PRIVATE
    safe_c_core
    safe_c_rt_static
)
if(UNIX AND NOT APPLE)
    target_link_libraries(safe_c_tests PRIVATE gcc_s)
endif()

target_include_directories(safe_c_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/runtime
)

target_compile_definitions(safe_c_tests PRIVATE
    TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

set_target_properties(safe_c_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME safe_c_unit_tests COMMAND safe_c_tests)
