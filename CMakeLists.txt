# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#

cmake_minimum_required (VERSION 3.25)

set(_VCPKG_TARGET_TRIPLET_ARCH x86)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("CrownLink")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF)

# Version single source of truth: the git tag (vMAJOR.MINOR.PATCH), see cmake/Version.cmake
include(cmake/Version.cmake)
crownlink_setup_version()

include(FetchContent)
FetchContent_Declare(
    storm_sdk
    GIT_REPOSITORY https://gitlab.com/Unveiling1561/storm_sdk.git
    GIT_TAG v1.1.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(storm_sdk)

# nlohmann/json: shared by SNP and tests
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

set(SIMDUTF_TESTS OFF)
set(SIMDUTF_TOOLS OFF)
FetchContent_Declare(
    simdutf
    GIT_REPOSITORY https://github.com/simdutf/simdutf.git
    GIT_TAG v7.3.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(simdutf)

FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG v1.0.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(concurrentqueue)

# C++ settings for first-party targets and everything fetched below this point;
# kept after the storm_sdk fetch so the vendored SDK keeps its own defaults
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Include sub-projects.
add_subdirectory("src")

# enable_testing must run at the top level or CTest ignores tests registered in subdirectories
enable_testing()
add_subdirectory("tests")

# caps pipeline: providers.toml -> caps.dat -> dist/CrownLink.snp, see cmake/Caps.cmake
# Runs after the subprojects so the dist step can reference the SNP target
include(cmake/Caps.cmake)
crownlink_setup_caps()

# CROWNLINK_DIST_FILE is the SNP output with caps.mpq appended, not the bare linker output
install(FILES "${CROWNLINK_DIST_FILE}"
    DESTINATION "C:/Cosmonarchy/Starcraft/"
)
