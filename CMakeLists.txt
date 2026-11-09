cmake_minimum_required(VERSION 3.20)
project(KronosDB CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Core Engine Library
add_library(kronos_core
    src/engine.cpp
    src/threadpool.cpp
    src/wal.cpp
    src/server.cpp
)
target_include_directories(kronos_core PUBLIC include)

# 2. Main Server Executable
add_executable(kronos_server src/main.cpp)
target_link_libraries(kronos_server PRIVATE kronos_core)

# 3. GoogleTest Integration
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(kronos_tests tests/kronos_test.cpp)
target_link_libraries(kronos_tests PRIVATE kronos_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(kronos_tests)