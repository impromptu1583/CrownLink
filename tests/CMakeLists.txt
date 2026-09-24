include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(SerializationTests SerializationTests.cpp)
target_include_directories(SerializationTests
    PRIVATE ${CMAKE_SOURCE_DIR}/src
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(SerializationTests
    PRIVATE Catch2::Catch2WithMain
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE storm::headers
    PRIVATE crownlink_version
    PRIVATE simdutf
)

add_test(SerializationTests SerializationTests)