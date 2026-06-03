cmake_minimum_required(VERSION 3.16)

project(ca_chip_simulation_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

add_executable(mini_memory_system
    examples/mini_memory_system.cpp
)
target_link_libraries(mini_memory_system PRIVATE time_engine)

