# Script-mode helper invoked by cmake/Version.cmake at configure time and on
# every build. Derives the version from the most recent git tag, rewrites
# crownlink/version.h only when its content changes, and records the numeric
# version in crownlink/version.stamp for the configure-time guard.

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "VersionStamp.cmake requires -DREPO_DIR")
endif()
if(NOT DEFINED TEMPLATE_FILE)
    message(FATAL_ERROR "VersionStamp.cmake requires -DTEMPLATE_FILE")
endif()
if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "VersionStamp.cmake requires -DOUTPUT_FILE")
endif()
if(NOT DEFINED STAMP_FILE)
    message(FATAL_ERROR "VersionStamp.cmake requires -DSTAMP_FILE")
endif()

set(version_major 0)
set(version_minor 0)
set(version_patch 0)
set(version_string "0.0.0-dev")

execute_process(
    COMMAND git describe --tags --match "v[0-9]*" --dirty
    WORKING_DIRECTORY "${REPO_DIR}"
    RESULT_VARIABLE describe_result
    OUTPUT_VARIABLE describe_output
    ERROR_VARIABLE describe_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

if(describe_result EQUAL 0)
    if(describe_output MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[0-9]+-g[0-9a-f]+)?(-dirty)?$")
        set(version_major "${CMAKE_MATCH_1}")
        set(version_minor "${CMAKE_MATCH_2}")
        set(version_patch "${CMAKE_MATCH_3}")
        set(version_string "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
        if(CMAKE_MATCH_4)
            string(APPEND version_string "${CMAKE_MATCH_4}")
        endif()
        if(CMAKE_MATCH_5)
            string(APPEND version_string "-dirty")
        endif()
    else()
        message(FATAL_ERROR
            "Nearest version tag '${describe_output}' does not follow the vMAJOR.MINOR.PATCH "
            "policy (e.g. v1.2.1). Delete or rename the tag, then reconfigure."
        )
    endif()
else()
    message(WARNING
        "git describe failed (${describe_error}); falling back to 0.0.0-dev. "
        "Create a vMAJOR.MINOR.PATCH tag to get a real version."
    )
endif()

math(EXPR version_numeric "${version_major} * 65536 + ${version_minor} * 256 + ${version_patch}")

# A changed numeric version (tag bump) means the configured build graph is stale;
# Ninja cannot reschedule translation units it already planned, so demand a reconfigure
if(DEFINED CONFIGURED_VERSION)
    if(NOT "${version_numeric}" EQUAL "${CONFIGURED_VERSION}")
        math(EXPR configured_major "${CONFIGURED_VERSION} / 65536")
        math(EXPR configured_minor "(${CONFIGURED_VERSION} % 65536) / 256")
        math(EXPR configured_patch "${CONFIGURED_VERSION} % 256")
        message(FATAL_ERROR
            "Version tag changed since CMake was configured "
            "(${configured_major}.${configured_minor}.${configured_patch} -> "
            "${version_major}.${version_minor}.${version_patch}). "
            "Re-run the CMake configure step, then build again."
        )
    endif()
endif()

file(READ "${TEMPLATE_FILE}" rendered)
string(REPLACE "@CL_VERSION_MAJOR@" "${version_major}" rendered "${rendered}")
string(REPLACE "@CL_VERSION_MINOR@" "${version_minor}" rendered "${rendered}")
string(REPLACE "@CL_VERSION_PATCH@" "${version_patch}" rendered "${rendered}")
string(REPLACE "@CL_VERSION_STRING@" "${version_string}" rendered "${rendered}")

# rc.exe fails with RC1004 when an included file does not end with a newline
string(LENGTH "${rendered}" rendered_length)
if(rendered_length GREATER 0)
    math(EXPR rendered_last "${rendered_length} - 1")
    string(SUBSTRING "${rendered}" ${rendered_last} 1 rendered_last_char)
    if(NOT rendered_last_char STREQUAL "\n")
        string(APPEND rendered "\n")
    endif()
endif()

if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" existing)
else()
    set(existing "")
endif()
if(NOT rendered STREQUAL existing)
    file(WRITE "${OUTPUT_FILE}" "${rendered}")
endif()

file(WRITE "${STAMP_FILE}" "${version_numeric}\n")