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

# Version single source of truth: the git tag (vMAJOR.MINOR.PATCH), see cmake/Version.cmake
include(cmake/Version.cmake)
crownlink_setup_version()

# storm_sdk: storm.dll ordinals + ABI types, pinned by tag
# Set FETCHCONTENT_SOURCE_DIR_STORM_SDK to a local checkout to skip the GitLab fetch
include(FetchContent)
FetchContent_Declare(
    storm_sdk
    GIT_REPOSITORY https://gitlab.com/Unveiling1561/storm_sdk.git
    GIT_TAG v1.0.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(storm_sdk)

# Include sub-projects.
add_subdirectory ("SNP")
#add_subdirectory ("Server")
add_subdirectory("CrowServe")

# caps pipeline: providers.toml -> caps.dat -> SNP/CrownLink.snp, see cmake/Caps.cmake
# Runs after the subprojects so the dist step can reference the SNP target
include(cmake/Caps.cmake)
crownlink_setup_caps()

# CROWNLINK_DIST_FILE is the SNP output with caps.mpq appended, not the bare linker output
install(FILES "${CROWNLINK_DIST_FILE}"
    DESTINATION "C:/Cosmonarchy/Starcraft/"
)