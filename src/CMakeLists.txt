set(MODULE_FILES
    "snp/SNPModule.cpp"
    "snp/SNPModule.h"
    "snp/CrownLink.def"
    "snp/AdvertisementManager.cpp"
    "snp/AdvertisementManager.h"
    "snp/DLLMain.cpp"
    "snp/JuiceAgent.h"
    "snp/JuiceAgent.cpp"
    "snp/JuiceManager.h"
    "snp/JuiceManager.cpp"
    "snp/Config.h"
    "snp/CrowServeManager.cpp"
    "snp/CrowServeManager.h"
    "snp/Logger.h"
    "snp/Globals.h"
    "snp/version.rc"
    "shared/types.h"
    "shared/StormTypes.h"
    "shared/Cbor.h"
    "shared/common.h"
    "crow_serve/CrowServe.h"
    "crow_serve/CrowServe.cpp"
    "crow_serve/CrownLinkProtocol.h"
    "crow_serve/CrownLinkProtocol.cpp"
    "crow_serve/P2PProtocol.h"
    "crow_serve/P2PProtocol.cpp"
)

# SHARED rather than MODULE: VS's CMake integration only offers EXECUTABLE and
# SHARED_LIBRARY targets as startup items, so a MODULE would drop the F5-the-game workflow.
# The generated import library is unused; exports come from CrownLink.def
add_library(SNP SHARED ${MODULE_FILES})

set(VCPKG_TARGET_TRIPLET "x86-windows-static")

set(outname "CrownLink")

set(SCEXE "C:\\Cosmonarchy\\Prerelease\\Cosmonarchy BW.exe")

set_target_properties(SNP
    PROPERTIES
        OUTPUT_NAME ${outname}
        SUFFIX ".snp"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/link"
        VS_DEBUGGER_COMMAND "${SCEXE}"
)

# src/ is the include root (shared/, crow_serve/, snp/); include/ holds the exported-API
# headers reached as crownlink/*.h
target_include_directories(SNP
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)

include(FetchContent)

set (NO_TESTS ON CACHE INTERNAL "Turn off libjuice tests")
set (NO_SERVER ON CACHE INTERNAL "Turn off libjuice server")
FetchContent_Declare(
    Juice
    # fork of paullouisageneau/libjuice adding Windows qWave Diffserv support, see
    # juice_send_diffserv
    GIT_REPOSITORY https://github.com/impromptu1583/libjuice.git
    GIT_TAG v1.7.3-qwave.1
)
FetchContent_MakeAvailable(Juice)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_STATIC OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(spdlog)

# To help visual studio find the includes
target_include_directories(SNP PRIVATE ${spdlog_SOURCE_DIR}/include/spdlog)
target_include_directories(SNP PRIVATE ${libjuice_SOURCE_DIR}/include/juice)
target_link_libraries(SNP
    PRIVATE ws2_32
    PRIVATE juice-static
    PRIVATE spdlog::spdlog_header_only
    PRIVATE storm::storm
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE crownlink_version
    PRIVATE simdutf
    PRIVATE concurrentqueue
)

target_compile_options(SNP PRIVATE "$<$<CONFIG:Release>:/Zi>")
target_link_options(SNP PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(SNP PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(SNP PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")

add_dependencies(SNP crownlink_version_refresh)