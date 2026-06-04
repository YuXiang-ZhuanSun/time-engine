cmake_minimum_required(VERSION 3.16)

project(ca_chip_simulation_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_library(time_engine
    src/time_engine/time_engine.cpp
)
target_include_directories(time_engine PUBLIC src)

add_executable(time_engine_tests
    tests/time_engine_tests.cpp
)
target_link_libraries(time_engine_tests PRIVATE time_engine)

enable_testing()
add_test(NAME time_engine_tests COMMAND time_engine_tests)

add_executable(memory_modeling_tests
    tests/memory_modeling_tests.cpp
)
target_link_libraries(memory_modeling_tests PRIVATE time_engine)
add_test(NAME memory_modeling_tests COMMAND memory_modeling_tests)

add_executable(mini_memory_system
    examples/mini_memory_system.cpp
)
target_link_libraries(mini_memory_system PRIVATE time_engine)

add_executable(memory_hierarchy
    examples/memory_hierarchy.cpp
)
target_link_libraries(memory_hierarchy PRIVATE time_engine)
