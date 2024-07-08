cmake_minimum_required(VERSION 3.25)
project(CrowServe)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED off)
set(CMAKE_CXX_EXTENSIONS off)

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

add_library(CrowServe STATIC CrowServe.hpp CrowServe.cpp)
target_link_libraries(CrowServe PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(CrowServe PUBLIC ${CMAKE_CURRENT_LIST_DIR})

enable_testing()
add_subdirectory(tests)