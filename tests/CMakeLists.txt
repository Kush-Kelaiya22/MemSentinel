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

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_test(
        NAME legacy_c89_compile
        COMMAND ${CMAKE_C_COMPILER}
            -std=c89 -pedantic -Wall -Wextra -Werror
            -DSAFE_C_ENABLE
            -fplugin=$<TARGET_FILE:safe_c>
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/tests/compile_pass/c89_legacy.c
            -o ${CMAKE_CURRENT_BINARY_DIR}/c89_legacy.o
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_test(
        NAME legacy_cpp98_compile
        COMMAND ${CMAKE_CXX_COMPILER}
            -std=c++98 -pedantic -Wall -Wextra -Werror
            -DSAFE_C_ENABLE
            -fplugin=$<TARGET_FILE:safe_c>
            -I${CMAKE_SOURCE_DIR}/include
            -c ${CMAKE_SOURCE_DIR}/tests/compile_pass/cpp98_legacy.cpp
            -o ${CMAKE_CURRENT_BINARY_DIR}/cpp98_legacy.o
    )
endif()
