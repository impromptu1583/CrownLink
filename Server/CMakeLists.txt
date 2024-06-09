
set(MODULE_FILES
	"CrownLinkServer.cpp"
	"CrownLinkServer.h"
	"../NetShared/StormTypes.h"
	"common.h"
)

add_executable(CLServer ${MODULE_FILES})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET CLServer PROPERTY CXX_STANDARD 20)
endif()


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/NetShared)
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(CLServer PRIVATE "$<$<CONFIG:Release>:/Zi>")
target_link_options(CLServer PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(CLServer PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(CLServer PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")

