cmake_minimum_required(VERSION 3.25)
project(CrowServe)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED off)
set(CMAKE_CXX_EXTENSIONS off)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

add_library(CrowServe STATIC
    CrowServe.h
    CrowServe.cpp
    CrownLinkProtocol.h
    CrownLinkProtocol.cpp
    P2PProtocol.h
    P2PProtocol.cpp
)
target_link_libraries(CrowServe PRIVATE nlohmann_json::nlohmann_json)
include_directories(${json_SOURCE_DIR}/include)
target_include_directories(CrowServe PUBLIC ${CMAKE_CURRENT_LIST_DIR})

enable_testing()
add_subdirectory(tests)