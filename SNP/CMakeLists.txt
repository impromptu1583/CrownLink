#set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(MODULE_FILES
	"SNPModule.cpp"
	"SNPModule.h"
	"CrownLink.def"
	"AdvertisementManager.cpp"
	"AdvertisementManager.h"
	"DLLMain.cpp"
	"JuiceAgent.h"
	"JuiceAgent.cpp"
	"JuiceManager.h"
	"JuiceManager.cpp"
    "../NetShared/StormTypes.h"
	"../NetShared/simdutf.cpp"
	"Config.h"	
	"CrowServeManager.cpp"
	"BWInteractions.h"
	"Logger.h"
	"CrowServeManager.h"
	"Globals.h"
)

add_library(SNP SHARED ${MODULE_FILES})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

set(VCPKG_TARGET_TRIPLET "x86-windows-static")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SNP PROPERTY CXX_STANDARD 23)
endif()

set(outname "CrownLink")

set(SCEXE "C:\\Cosmonarchy\\Prerelease\\Cosmonarchy BW.exe")

set_target_properties(SNP
	PROPERTIES
		OUTPUT_NAME ${outname}
		SUFFIX ".snp"
		VS_DEBUGGER_COMMAND "${SCEXE}"
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/NetShared)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vendor)

include(FetchContent)

set (NO_TESTS ON CACHE INTERNAL "Turn off libjuice tests")
set (NO_SERVER ON CACHE INTERNAL "Turn off libjuice server")
FetchContent_Declare(
	Juice
	GIT_REPOSITORY https://github.com/paullouisageneau/libjuice.git
	GIT_TAG v1.7.0
)
FetchContent_MakeAvailable(Juice)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)
target_link_libraries(SNP PRIVATE nlohmann_json::nlohmann_json)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_STATIC OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.16.0
)
FetchContent_MakeAvailable(spdlog)

# To help visual studio find the includes
target_include_directories(SNP PRIVATE ${spdlog_SOURCE_DIR}/include/spdlog)
target_include_directories(SNP PRIVATE ${libjuice_SOURCE_DIR}/include/juice)
target_link_libraries(SNP
	PRIVATE CrowServe
	PRIVATE ws2_32
	PRIVATE juice-static
	PRIVATE spdlog::spdlog_header_only
)

target_compile_options(SNP PRIVATE "$<$<CONFIG:Release>:/Zi>")
target_link_options(SNP PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(SNP PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(SNP PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")

# copy CAPS MPQ into SNP
message(STATUS "Copying ${CMAKE_SOURCE_DIR}/SNP/caps.mpq at the end of SNP")
#set(mpqfile "${CMAKE_SOURCE_DIR}/SNP/caps.mpq")

add_dependencies(SNP CrownMPQ)

add_custom_command( TARGET SNP
	POST_BUILD
	COMMAND cmd /C "copy /Y /B \"$<SHELL_PATH:$<TARGET_FILE:SNP>>\" + \"$<SHELL_PATH:${mpqfile}>\" \"$<SHELL_PATH:$<TARGET_FILE:SNP>>\""
)

install(TARGETS SNP
	DESTINATION "C:/Cosmonarchy/Starcraft/"
)

# del "C:\Cosmonarchy\Starcraft\CrownLink.snp" && mklink "C:\Cosmonarchy\Starcraft\CrownLink.snp" "C:\Users\Jesse\source\repos\CrownLink\out\build\x86-debug\SNP\CrownLink.snp
# del "C:\Cosmonarchy\Starcraft\CrownLink.snp" && mklink "C:\Cosmonarchy\Starcraft\CrownLink.snp" "C:\Users\Jesse\source\repos\CrownLink\out\build\x86-release\SNP\CrownLink.snp
