#SET(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(MODULE_FILES
	Storm/storm.h
	Storm/storm.cpp
	Util/MemoryFrame.cpp
	Util/MemoryFrame.h
	Util/Types.h
	Util/Exceptions.h
	Util/Exceptions.cpp
	Output.cpp
	Output.h
	SNPModule.cpp
	SNPModule.h
	SNPNetwork.h
	CriticalSection.h
	CriticalSection.cpp
	caps.mpq
	SNPModule.def
	CrownLink.cpp
	CrownLink.h
	DLLMain.cpp
	Signaling.h
	Signaling.cpp
	JuiceManager.h
	JuiceManager.cpp
	"include/base64.hpp"
	"include/json.hpp"
    SNETADDR.h
	"Config.h"
	"Common.h"
	ThQueue.h
	Logger.h
)

add_library(SNP SHARED ${MODULE_FILES})

set(TEST_FILES
	Util/MemoryFrame.cpp
	Util/MemoryFrame.h
	Util/Exceptions.h
	Util/Exceptions.cpp
	signaling.h
	signaling.cpp
	JuiceManager.h
	JuiceManager.cpp
	SignallingTest.cpp
	json.hpp
    SNETADDR.h
	"include/base64.hpp"
	ThQueue/Logger.h
)

#add_executable (SIGNALTEST ${TEST_FILES})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SNP PROPERTY CXX_STANDARD 20)
#  set_property(TARGET SIGNALTEST PROPERTY CXX_STANDARD 20)
endif()

#target_link_libraries(SIGNALTEST
#	PUBLIC ws2_32
#	PRIVATE juice-static
#)

set(outname "CrownLink")

set_target_properties(SNP
	PROPERTIES
		OUTPUT_NAME ${outname}
		SUFFIX ".snp"
		VS_DEBUGGER_COMMAND "${SCEXE}"
)
include_directories(${CMAKE_SOURCE_DIR}/SNP/Storm)
include_directories(${CMAKE_SOURCE_DIR}/SNP/Util)
include_directories(${CMAKE_SOURCE_DIR}/SNP/include)

include(FetchContent)
#juice
FetchContent_Declare(
	Juice
	GIT_REPOSITORY https://github.com/paullouisageneau/libjuice.git
	GIT_TAG v1.4.1
)

FetchContent_MakeAvailable(Juice)
target_include_directories(SNP PUBLIC ${libjuice_SOURCE_DIR}/include/juice)
#target_include_directories(SIGNALTEST PUBLIC ${libjuice_SOURCE_DIR}/include/juice)
target_link_libraries(SNP
#	PUBLIC wsock32
	PUBLIC ws2_32
	PRIVATE juice-static
)

if (MSVC)
    #target_compile_options(SNP PRIVATE /Zc:auto-)
	#target_compile_options(SIGNALTEST PRIVATE /Zc:auto-)
endif ()

# copy CAPS MPQ into SNP

message(STATUS "Copying ${CMAKE_SOURCE_DIR}/SNP/caps.mpq at the end of SNP")
set ( mpqfile "${CMAKE_SOURCE_DIR}/SNP/caps.mpq" )

add_custom_command( TARGET SNP
	POST_BUILD
	COMMAND cmd /C "copy /Y /B \"$<SHELL_PATH:$<TARGET_FILE:SNP>>\" + \"$<SHELL_PATH:${mpqfile}>\" \"$<SHELL_PATH:$<TARGET_FILE:SNP>>\""
)

install(TARGETS SNP
	DESTINATION "C:/Cosmonarchy/Starcraft/"
)

# del "C:\Cosmonarchy\Starcraft\CrownLink.snp" && mklink "C:\Cosmonarchy\Starcraft\CrownLink.snp" "C:\Users\Jesse\source\repos\SNPJuice\out\build\x86-debug\SNP\CrownLink.snp
# del "C:\Cosmonarchy\Starcraft\CrownLink.snp" && mklink "C:\Cosmonarchy\Starcraft\CrownLink.snp" "C:\Users\Jesse\source\repos\SNPJuice\out\build\x86-release\SNP\CrownLink.snp
# mklink "C:\Cosmonarchy\Starcraft\CrownLink_config.json" "C:\Users\Jesse\source\repos\SNPJuice\SNP\CrownLink_config.json