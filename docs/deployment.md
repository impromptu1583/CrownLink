# Deploying a CrownLink release

The shippable artifact is `out/build/x86-release/dist/CrownLink.snp` — the linked
DLL with `caps.mpq` appended by the `crownlink_dist` target. The bare linker
output in `out/build/x86-release/link/` is **not** a complete plugin; storm
loads provider caps from the appended MPQ, so only the `dist/` file is usable.

## Where the version comes from

The single source of truth is the most recent git tag, read with
`git describe --tags --match "v[0-9]*" --dirty` at configure time and refreshed
on every build (`cmake/Version.cmake`, `cmake/VersionStamp.cmake`).

The version string has three shapes:

| state of the repo                       | version string       |
|-----------------------------------------|----------------------|
| exactly on a tag, clean working tree    | `1.2.3`              |
| commits after the nearest tag           | `1.2.3-4-g1a2b3c4`   |
| uncommitted changes in the working tree | `1.2.3-dirty`        |

It surfaces in three places, all generated from the same tag:

- `crownlink/version.h` (`CL_VERSION_STRING`) — logged by the plugin at startup
  ("CrownLink v1.2.3 initializing") and reported to crowserve in
  `ConnectionRequest.crownlink_version`
- `snp/version.rc` — the file properties of the `.snp`
- `caps.dat` inside the appended MPQ — baked in by `scripts/build_caps.py`

A `-dirty` or `-N-g<hash>` suffix therefore ships in the caps data where
players and the server can see it. For a release you want the bare
`MAJOR.MINOR.PATCH` form, which means: **tag on the release commit, no
uncommitted changes**.

## Release checklist

1. **Clean working tree.** Commit everything; verify with `git status` — any
   modification, staged or not, produces a `-dirty` version.

2. **Tag the release commit.** Tags must match `vMAJOR.MINOR.PATCH` exactly;
   anything else fails the configure with a policy error.

   ```
   git tag -a v1.2.4 -m "CrownLink 1.2.4"
   ```

3. **Push the commit and the tag.**

   ```
   git push origin main v1.2.4
   ```

4. **Reconfigure.** This step is not optional: when the numeric version
   changes, the version stamp deliberately aborts any build that was planned
   with the old version, because Ninja cannot reschedule already-planned
   translation units mid-build. Building without reconfiguring fails with:

   ```
   Version tag changed since CMake was configured (1.2.3 -> 1.2.4).
   Re-run the CMake configure step, then build again.
   ```

   That message means exactly what it says. From the command line:

   ```
   cmake --preset x86-release
   ```

   In Visual Studio: CMake menu → *Delete Cache and Reconfigure* (or touch
   `CMakeLists.txt` to trigger an automatic reconfigure).

   A fresh configure also needs network access the first time — dependencies
   (storm_sdk, reflect-cpp, jsoncons, simdutf, spdlog, juice, catch2,
   concurrentqueue) are FetchContent'd on demand.

5. **Build.** VS: select the *x86 Release* preset and build. Or:

   ```
   cmake --build out/build/x86-release
   ```

6. **Run the tests.**

   ```
   ctest --test-dir out/build/x86-release
   ```

   If the wire protocol changed at all, also run the crowserve suite — the
   fixture tests on both sides pin the client↔server CBOR contract:

   ```
   cd ../crowserve && go test ./...
   ```

7. **Verify the version before shipping.** Check the log line or the file
   properties of `dist/CrownLink.snp`; the version string must be the bare tag
   with no suffix. Rebuild from a dirty tree rather than shipping `-dirty`.

8. **Optional: install locally.** The install step copies the dist file next
   to the game:

   ```
   cmake --install out/build/x86-release
   ```

   (destination: `C:\Cosmonarchy\Starcraft\` — see the `install()` call in the
   top-level `CMakeLists.txt`).

9. **Publish.** Attach `out/build/x86-release/dist/CrownLink.snp` to the
   GitHub release for the tag. The README points players at the releases page.