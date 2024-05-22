set(MODULE_FILES
	Exceptions.cpp
	Exceptions.h
	MemoryFrame.cpp
	MemoryFrame.h
	Mutex.cpp
	Mutex.h
	Path.h
	RemoteProcess.cpp
	RemoteProcess.h
	RemoteProcessID.h
	Sha1.cpp
	Sha1.h
	SharedMemory.cpp
	SharedMemory.h
	SharedStructure.h
	StringUtil.h
	Types.h
	Clamp.h
	CMakeLists.txt
	Convenience.h
)

add_library(Util STATIC ${MODULE_FILES})

target_include_directories(Util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
install(TARGETS Util DESTINATION Util-bin)
install(FILES storm.h DESTINATION Util-include)