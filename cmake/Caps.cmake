# caps pipeline: providers.toml plus the git-tag version string go in, dist/CrownLink.snp
# (the dll with caps.mpq appended) comes out; the raw linker output lives in link/
#   configure time: build_caps.py writes caps.dat and generated/crownlink/provider_data.h
#   build time:     caps.dat regenerates when providers.toml or the version changes; the
#                   crownlink_dist target packs it into caps.mpq with mpqcli and appends it
#                   to the linked dll
# provider_data.h is consumed by compiled code, so providers.toml edits must go through a
# reconfigure; the crownlink_caps_guard target fails the build when the toml changed first

include(ExternalProject)

function(crownlink_setup_caps)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)

    set(providers_toml "${CMAKE_SOURCE_DIR}/scripts/providers.toml")
    set(build_caps_py "${CMAKE_SOURCE_DIR}/scripts/build_caps.py")
    set(generated_dir "${CMAKE_BINARY_DIR}/generated")
    set(version_h "${generated_dir}/crownlink/version.h")
    set(provider_data_h "${generated_dir}/crownlink/provider_data.h")
    set(caps_dir "${CMAKE_BINARY_DIR}/caps")
    set(caps_dat "${caps_dir}/caps.dat")
    set(caps_mpq "${caps_dir}/caps.mpq")
    set(caps_mpq_tmp "${caps_dir}/caps.mpq.tmp")
    set(providers_stamp "${caps_dir}/providers.toml.sha")
    set(mpqcli_exe "${caps_dir}/mpqcli.exe")
    set(append_script "${CMAKE_SOURCE_DIR}/scripts/append_file.py")
    set(snp_output "${CMAKE_BINARY_DIR}/dist/CrownLink.snp")

    # Configure-time generation so provider_data.h exists before the first compile
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${build_caps_py}"
            --providers "${providers_toml}"
            --version-header "${version_h}"
            --dat-out "${caps_dat}"
            --header-out "${provider_data_h}"
            --stamp "${providers_stamp}"
        RESULT_VARIABLE caps_configure_result
    )
    if(NOT caps_configure_result EQUAL 0)
        message(FATAL_ERROR "Generating caps.dat / provider_data.h failed")
    endif()

    # Rebuild caps.dat when the provider list, the generator or the version string changes
    add_custom_command(
        OUTPUT "${caps_dat}"
        COMMAND "${Python3_EXECUTABLE}" "${build_caps_py}"
            --providers "${providers_toml}"
            --version-header "${version_h}"
            --dat-out "${caps_dat}"
        DEPENDS "${providers_toml}" "${build_caps_py}" "${version_h}"
        VERBATIM
    )

    # Fails the build when providers.toml changed since configure, because provider_data.h
    # (provider ids, MaxPacketSize) is baked into compiled code at configure time
    add_custom_target(crownlink_caps_guard ALL
        COMMAND "${Python3_EXECUTABLE}" "${build_caps_py}"
            --providers "${providers_toml}"
            --verify
            --stamp "${providers_stamp}"
        COMMENT "Verifying providers.toml matches the configured copy"
        VERBATIM
    )

    # Isolated project instead of FetchContent as it assumes it's the top-level project
    # It also includes StormLib whose 'storm' target collides
    ExternalProject_Add(mpqcli
        GIT_REPOSITORY https://github.com/thegraydot/mpqcli.git
        GIT_TAG v0.11.0
        GIT_SHALLOW TRUE
        PREFIX "${caps_dir}/mpqcli-prefix"
        CMAKE_CACHE_ARGS -DCMAKE_BUILD_TYPE:STRING=Release
        INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy "<BINARY_DIR>/bin/mpqcli.exe" "${mpqcli_exe}"
        INSTALL_BYPRODUCTS "${mpqcli_exe}"
    )

    # --flags 0 stores caps.dat raw (MPQ_FILE_EXISTS only): every game profile compresses
    # and encrypts added files by default, which the game rejected when loading caps.dat.
    # --sign is mandatory: storm.dll's snp_load_mpq gates the caps.dat read on
    # SFileAuthenticateArchive, and an unsigned archive fails that check
    # The pack+append is one command sequence so a fresh caps.dat always lands in the
    # SNP output within the same build; the standalone caps.mpq stays available for tooling
    add_custom_command(
        OUTPUT "${snp_output}"
        COMMAND "${CMAKE_COMMAND}" -E remove "${caps_mpq_tmp}"
        COMMAND "${mpqcli_exe}" create --game starcraft --flags 0 --sign "${caps_dat}" --output "${caps_mpq_tmp}" --path caps.dat
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${caps_mpq_tmp}" "${caps_mpq}"
        COMMAND "${Python3_EXECUTABLE}" "${append_script}" "$<TARGET_FILE:SNP>" "${caps_mpq_tmp}" "${snp_output}"
        DEPENDS "$<TARGET_FILE:SNP>" "${caps_dat}" "${mpqcli_exe}"
        BYPRODUCTS "${caps_mpq}"
        COMMENT "Packing CrownLink.snp with caps.mpq"
        VERBATIM
    )
    add_custom_target(crownlink_dist ALL DEPENDS "${snp_output}")
    add_dependencies(crownlink_dist mpqcli crownlink_caps_guard)

    # Consumed by the install() call in the top-level CMakeLists.txt
    set(CROWNLINK_DIST_FILE "${snp_output}" PARENT_SCOPE)
endfunction()