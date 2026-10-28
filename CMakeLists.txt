cmake_minimum_required(VERSION 3.16)
project(KronosDB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This tells the compiler to look for the included directory in the include folder
include_directories(include)

add_executable(kronos_server 
    src/main.cpp 
    src/engine.cpp
    src/threadpool.cpp
    src/wal.cpp
)