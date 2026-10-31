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

# --long always appends -<distance>-g<hash>, so a hyphenated prerelease such as
# v1.2.4-rc.1 can be split from the describe suffix unambiguously
execute_process(
    COMMAND git describe --tags --long --match "v[0-9]*" --dirty
    WORKING_DIRECTORY "${REPO_DIR}"
    RESULT_VARIABLE describe_result
    OUTPUT_VARIABLE describe_output
    ERROR_VARIABLE describe_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

if(describe_result EQUAL 0)
    set(describe_rest "${describe_output}")
    set(dirty FALSE)
    if(describe_rest MATCHES "^(.*)-dirty$")
        set(dirty TRUE)
        set(describe_rest "${CMAKE_MATCH_1}")
    endif()
    if(NOT describe_rest MATCHES "^(.*)-([0-9]+)-g([0-9a-f]+)$")
        message(FATAL_ERROR "Unexpected git describe output '${describe_output}'")
    endif()
    set(tag "${CMAKE_MATCH_1}")
    set(commits_since_tag "${CMAKE_MATCH_2}")
    set(commit_hash "${CMAKE_MATCH_3}")

    if(NOT tag MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*)?$")
        message(FATAL_ERROR
            "Nearest version tag '${tag}' does not follow the vMAJOR.MINOR.PATCH[-PRERELEASE] "
            "policy (e.g. v1.2.1 or v1.2.4-rc.1). Delete or rename the tag, then reconfigure."
        )
    endif()
    set(version_major "${CMAKE_MATCH_1}")
    set(version_minor "${CMAKE_MATCH_2}")
    set(version_patch "${CMAKE_MATCH_3}")
    string(SUBSTRING "${tag}" 1 -1 version_string)

    # Semver build metadata: untagged or modified builds sort equal to their tag and
    # never look like a release, e.g. 1.2.4-rc.1+3.g1a2b3c4.dirty
    set(build_metadata "")
    if(NOT commits_since_tag EQUAL 0)
        list(APPEND build_metadata "${commits_since_tag}" "g${commit_hash}")
    endif()
    if(dirty)
        list(APPEND build_metadata "dirty")
    endif()
    if(build_metadata)
        list(JOIN build_metadata "." build_metadata)
        string(APPEND version_string "+${build_metadata}")
    endif()
else()
    message(WARNING
        "git describe failed (${describe_error}); falling back to 0.0.0-dev. "
        "Create a vMAJOR.MINOR.PATCH[-PRERELEASE] tag to get a real version."
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