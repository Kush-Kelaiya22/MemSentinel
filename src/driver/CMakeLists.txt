add_executable(memsentinel
    main.cpp
)

target_link_libraries(memsentinel PRIVATE
    memcheck_core
)

target_include_directories(memsentinel PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core
)

set_target_properties(memsentinel PROPERTIES
    OUTPUT_NAME "memsentinel"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
