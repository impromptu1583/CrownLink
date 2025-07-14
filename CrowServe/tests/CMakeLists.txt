Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

add_executable(CrowServeTest CrowServeTest.cpp ../../NetShared/simdutf.cpp)
target_link_libraries(CrowServeTest PRIVATE CrowServe Catch2::Catch2WithMain)

add_test(CrowServeTest CrowServeTest)
# copy paste to add more tests