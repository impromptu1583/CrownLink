include(FetchContent)

# Catch2 has defaulted to a shared build since v3.4.0; force static so the test exe has no
# runtime DLL dependencies
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(CrowServeTest CrowServeTest.cpp ../../NetShared/simdutf.cpp)
target_include_directories(CrowServeTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(CrowServeTest
    PRIVATE Catch2::Catch2WithMain
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE storm::headers
    PRIVATE crownlink_version
)

add_test(CrowServeTest CrowServeTest)