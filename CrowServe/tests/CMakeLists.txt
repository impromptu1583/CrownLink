Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2
  GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(ExampleTest ExampleTest.cpp)
target_link_libraries(ExampleTest PRIVATE CrowServe Catch2::Catch2WithMain)
add_test(ExampleTest ExampleTest)

# copy paste to add more tests