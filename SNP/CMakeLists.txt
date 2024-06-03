#set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(MODULE_FILES
	"bwapi/Storm/storm.h"
	"bwapi/Storm/storm.cpp"
	"bwapi/Util/MemoryFrame.cpp"
	"bwapi/Util/MemoryFrame.h"
	"bwapi/Util/Exceptions.h"
	"bwapi/Util/Exceptions.cpp"
	"include/base64.hpp"
	"include/json.hpp"
	
	"SNPModule.cpp"
	"SNPModule.h"
	"SNPModule.def"
	"CrownLink.cpp"
	"CrownLink.h"
	"DLLMain.cpp"
	"Signaling.h"
	"Signaling.cpp"
	"JuiceManager.h"
	"JuiceManager.cpp"
	"JuiceAgent.h"
	"JuiceAgent.cpp"
    "NetAddress.h"
	"Config.h"
	
	"Common.h"
	"ThQueue.h"
	#"Logger.h"
	"caps.mpq")

add_library(SNP SHARED ${MODULE_FILES})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(VCPKG_TARGET_TRIPLET "x86-windows-static")


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SNP PROPERTY CXX_STANDARD 20)
endif()

set(outname "CrownLink")

set_target_properties(SNP
	PROPERTIES
		OUTPUT_NAME ${outname}
		SUFFIX ".snp"
		VS_DEBUGGER_COMMAND "${SCEXE}"
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/bwapi)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/bwapi/Storm)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/bwapi/Util)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

include(FetchContent)

# juice
FetchContent_Declare(
	Juice
	GIT_REPOSITORY https://github.com/paullouisageneau/libjuice.git
	GIT_TAG v1.4.1
)
FetchContent_MakeAvailable(Juice)
target_include_directories(SNP PUBLIC ${libjuice_SOURCE_DIR}/include/juice)
target_link_libraries(SNP
	PUBLIC ws2_32
	PRIVATE juice-static
)

target_compile_options(SNP PRIVATE "$<$<CONFIG:Release>:/Zi>")
target_link_options(SNP PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(SNP PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(SNP PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")



# copy CAPS MPQ into SNP
message(STATUS "Copying ${CMAKE_SOURCE_DIR}/SNP/caps.mpq at the end of SNP")
set(mpqfile "${CMAKE_SOURCE_DIR}/SNP/caps.mpq")

add_custom_command( TARGET SNP
	POST_BUILD
	COMMAND cmd /C "copy /Y /B \"$<SHELL_PATH:$<TARGET_FILE:SNP>>\" + \"$<SHELL_PATH:${mpqfile}>\" \"$<SHELL_PATH:$<TARGET_FILE:SNP>>\""
)

install(TARGETS SNP
	DESTINATION "C:/Cosmonarchy/Starcraft/"
)

# del "C:\Cosmonarchy\Starcraft\CrownLink.snp" && mklink "C:\Cosmonarchy\Starcraft\CrownLink.snp" "C:\Users\Jesse\source\repos\SNPJuice\out\build\x86-debug\SNP\CrownLink.snp
# del "C:\Cosmonarchy\Starcraft\CrownLink.snp" && mklink "C:\Cosmonarchy\Starcraft\CrownLink.snp" "C:\Users\Jesse\source\repos\SNPJuice\out\build\x86-release\SNP\CrownLink.snp
