set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(StormLib STATIC IMPORTED)
set_property(TARGET StormLib PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/StormLibDAS.lib)

add_executable(CrownMPQ CrownMPQ.cpp)

target_link_libraries(CrownMPQ PRIVATE StormLib)

add_custom_command( TARGET CrownMPQ
	POST_BUILD
	COMMAND cmd /C "$<SHELL_PATH:$<TARGET_FILE:CrownMPQ>> $<SHELL_PATH:${mpqfile}>"
)
