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
	SettingsDialog.cpp
	SettingsDialog.h
	SNPModule.cpp
	SNPModule.h
	SNPNetwork.h
	UDPSocket.h
	CriticalSection.cpp
	UI.rc
	caps.mpq
	resource.h
	SNPModule.def
	UDPSocket.cpp
	CriticalSection.h
	DirectIP.cpp
	DirectIP.h
	DLLMain.cpp
	signaling.h
	signaling.cpp
)

add_library (SNP SHARED ${MODULE_FILES})

set(TEST_FILES
	Util/MemoryFrame.cpp
	Util/MemoryFrame.h
	Util/Types.h
	Util/Exceptions.h
	Util/Exceptions.cpp
	UDPSocket.h
	UDPSocket.cpp
	"SignallingTest.cpp"
)

add_executable (UDPTEST ${TEST_FILES})
target_link_libraries(UDPTEST ws2_32)


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SNP PROPERTY CXX_STANDARD 20)
endif()

set(outname "JuiceSNP")

set_target_properties (SNP
	PROPERTIES
		OUTPUT_NAME ${outname}
		SUFFIX ".snp"
		VS_DEBUGGER_COMMAND "${SCEXE}"
)
include_directories(${CMAKE_SOURCE_DIR}/SNP/Storm)
include_directories(${CMAKE_SOURCE_DIR}/SNP/Util)


include(FetchContent)
#juice
FetchContent_Declare(
	Juice
	GIT_REPOSITORY https://github.com/paullouisageneau/libjuice.git
	GIT_TAG v1.4.1
)

FetchContent_MakeAvailable(Juice)
target_include_directories(SNP PUBLIC ${libjuice_SOURCE_DIR}/include/juice)

target_link_libraries(SNP
#	PUBLIC wsock32
	PUBLIC ws2_32
	PRIVATE juice-static
)



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



# mklink linked.snp "C:\Users\Jesse\source\repos\SNPJuice\out\build\x86-debug\SNP\JuiceSNP.snp