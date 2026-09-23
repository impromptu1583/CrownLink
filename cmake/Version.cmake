# Version single source of truth: the git tag (vMAJOR.MINOR.PATCH).
# crownlink/version.h is generated at configure time and refreshed on every build;
# VersionStamp.cmake rewrites it only when its content changes, so incremental
# builds stay incremental. When the numeric version changes (a new tag), the
# stamp aborts the build and asks for a reconfigure, because Ninja cannot
# reschedule translation units that were already planned in the current run.

set(crownlink_version_module_dir "${CMAKE_CURRENT_LIST_DIR}")

function(crownlink_setup_version)
    set(generated_dir "${CMAKE_BINARY_DIR}/generated")
    set(version_h "${generated_dir}/crownlink/version.h")
    set(version_stamp "${generated_dir}/crownlink/version.stamp")

    # Configure-time generation so the header exists before the first compile
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DREPO_DIR=${CMAKE_SOURCE_DIR}"
            "-DTEMPLATE_FILE=${crownlink_version_module_dir}/version.h.in"
            "-DOUTPUT_FILE=${version_h}"
            "-DSTAMP_FILE=${version_stamp}"
            -P "${crownlink_version_module_dir}/VersionStamp.cmake"
        RESULT_VARIABLE stamp_result
    )
    if(NOT stamp_result EQUAL 0)
        message(FATAL_ERROR "Generating version.h failed")
    endif()

    file(READ "${version_stamp}" configured_version)
    string(STRIP "${configured_version}" configured_version)

    # Carries the generated include directory to every target that links it
    add_library(crownlink_version INTERFACE)
    target_include_directories(crownlink_version INTERFACE "${generated_dir}")

    add_custom_target(crownlink_version_refresh ALL
        COMMAND "${CMAKE_COMMAND}"
            "-DREPO_DIR=${CMAKE_SOURCE_DIR}"
            "-DTEMPLATE_FILE=${crownlink_version_module_dir}/version.h.in"
            "-DOUTPUT_FILE=${version_h}"
            "-DSTAMP_FILE=${version_stamp}"
            "-DCONFIGURED_VERSION=${configured_version}"
            -P "${crownlink_version_module_dir}/VersionStamp.cmake"
        COMMENT "Refreshing crownlink/version.h from the latest git tag"
        VERBATIM
    )
endfunction()