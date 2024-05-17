cmake_minimum_required(VERSION 3.25)
project(ThQueue)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_MAKEFILE ON)

add_executable(test_queue test_queue.cpp)
add_executable(test_logger test_logger.cpp)