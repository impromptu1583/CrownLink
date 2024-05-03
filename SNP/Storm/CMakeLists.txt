set(MODULE_FILES
	storm.cpp
	storm.h
)

add_library(Storm STATIC ${MODULE_FILES})

target_include_directories(Storm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
install(TARGETS Storm DESTINATION Storm-bin)
install(FILES storm.h DESTINATION Storm-include)