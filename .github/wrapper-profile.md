# CesiumC wrapper profile

What is specific to this repository. The shared half is
[`docs/cpp-wrapper-conventions.md`](https://github.com/EvergineTeam/Evergine.Bindings/blob/main/docs/cpp-wrapper-conventions.md)
in the toolbox, and `cpp-wrapper-porter` reads both.

That document describes how wrappers here are written. **It does not authorise rewriting this
one.** A bump repairs what upstream broke; anything else belongs in a separate change.

## Identifiers

| | |
|---|---|
| Function naming | `cesium_<noun>_<verb>`, **snake_case**, no domain segment |
| Export macro | `CESIUM_API`, gated on `CESIUM_NATIVE_C_EXPORTS` |
| Error macros | `CESIUM_TRY_BEGIN` / `CESIUM_TRY_END`, in `src/cesium_errors_internal.h` |
| Last-error accessors | `cesium_get_last_error`, `cesium_clear_last_error` |
| Handle types | `CesiumTileset`, `CesiumEllipsoid` — PascalCase opaque structs |
| Version macro | `CESIUMC_CESIUM_NATIVE_VERSION` in `include/cesium/cesium_common.h` |

snake_case throughout, which is the opposite of JoltPhysicsC's `JoltC_Type_Method`. Both are
deliberate and neither is being migrated. Do not carry a habit across.

## Overloads

**Semantic suffixes**, not numeric ones: `cesium_tileset_create_from_url`,
`cesium_tileset_create_from_ion`, `cesium_cartographic_from_degrees`,
`cesium_view_state_create_from_matrices`.

If upstream adds a constructor, name it for what distinguishes it. `_create2` is JoltPhysicsC's
answer to the same question and is wrong here.

## Paths

| | |
|---|---|
| Public headers | `include/cesium/` |
| Implementation | `src/` |
| Internal headers | `src/*_internal.h`, `src/cesium_wrappers.h` — not shipped |
| Tests | `tests/test_cesium_native_c.cpp` |
| Build | `CMakeLists.txt` at the root |
| Upstream submodule | `cesium-native/` |
| Overlay triplets | `triplets/` |
| Submodule patches | `patches/` |

## Scope contract

**Not exhaustive, and deliberately so.** 169 functions covering 6 of cesium-native's 23
libraries: tileset selection and traversal, glTF reading, geospatial maths, raster overlays,
Ion access, and the async plumbing those need.

The gap is scope rather than holes: there are **no stub functions**. What is declared works.

So a release that adds API is usually not a gap to fill. Report it; do not implement it.

## Applying a bump

**Three edits, not one.** Missing the second or third produces a build that succeeds against
the old dependency set, which is worse than one that fails.

1. **The submodule pointer**, `cesium-native/`, to the new tag.
2. **The vcpkg baseline** in `vcpkg-configuration.json`, to the commit upstream's own
   `vcpkg-configuration.json` records for that release. cesium-native pins a baseline; taking
   its libraries without its dependency versions is how a bump compiles and then misbehaves.
3. **`target_link_libraries` in `CMakeLists.txt`**, if upstream added or removed a library. It
   is an explicit list of fifteen, not a glob.

And `vcpkg.json` is a **second copy of cesium-native's dependency list**, which applies its own
platform guards in CMake rather than in its manifest. When the bump changes upstream's
`PACKAGES_PRIVATE` block, diff it against ours. Three guards already exist there because they
were found the hard way: `curl` and `cpp-httplib` are unusable in a browser, and `ktx` needs
`default-features: false` because the port enables its `js` feature by default on Emscripten and
those wrappers do not compile.

Also update **`CESIUMC_CESIUM_NATIVE_VERSION`** in `include/cesium/cesium_common.h`, all four
defines, and `upstream.release.current` in `binding.yml`. Nothing checks that they agree.

### The submodule pointer is not yours to deliver

Record the release in `binding.yml` and restore the pointer before you finish
(`git submodule update --init --force cesium-native`). `wrapper-submodule-bump` moves it on
your branch afterwards. A patch containing a gitlink does not merge with a warning — the whole
pull request degrades into an issue.

The baseline, the library list and the manifest **are** ordinary file changes: include them.

## Building and testing

```
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure --no-tests=error
```

`VCPKG_OVERLAY_TRIPLETS` must point at `triplets/`; those override the built-ins with
Release-only builds, which roughly halves dependency compile time.

Expect the first configure to take **25 to 30 minutes** without a warm binary cache: thirty
dependencies, built from source. That is normal, not a hang.

29 tests, all offline. `CESIUM_ION_TOKEN` empty in CI skips the Ion-dependent ones.

## Platforms

Seven targets, and they are not interchangeable when judging a repair.

| | |
|---|---|
| Desktop | `win-x64`, `win-arm64`, `linux-x64`, `linux-arm64`, `osx-x64`, `osx-arm64` |
| Web | `browser-wasm` |
| Not yet | iOS, Android — deliberately deferred |

**browser-wasm is the one that will surprise you.** It is single-threaded and has no libcurl,
so two implementations differ by `#if`:

- `src/cesium_async.cpp` — `DeferredTaskProcessor` instead of `ThreadPoolTaskProcessor`,
  selected by `defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)`. `std::thread`
  aborts there.
- `src/cesium_asset_accessor.cpp` — a placeholder accessor that fails every request, selected
  by `CESIUMC_NO_CURL`, which CMake defines from `CESIUM_DISABLE_CURL`. **A real
  `fetch`-based accessor is still to be written.**

If a repair touches either file, check both halves compile. The wasm leg is the last to run and
the easiest to break without noticing.

## Version

`CESIUMC_CESIUM_NATIVE_VERSION` and its three numeric defines in
`include/cesium/cesium_common.h`. It must equal `upstream.release.current`.

It exists because it is the only version statement a consumer can read without cloning this
repository. Cesium.NET has no other way to say which Cesium it binds to.

## Known local quirks

- **cesium-native detects wasm by the *name* of `CMAKE_TOOLCHAIN_FILE`.** Under this project
  that name is `vcpkg.cmake`, so its check never fires; the root `CMakeLists.txt` sets
  `CESIUM_TARGET_WASM` and `CESIUM_DISABLE_CURL` by hand from `EMSCRIPTEN`. Upstream's own
  `set(CESIUM_DISABLE_CURL ON)` is directory-scope and never reaches this scope.
- **`CESIUM_USE_EZVCPKG` defaults OFF here**, because a parent supplying `vcpkg.cmake` turns it
  off. That puts vcpkg in manifest mode, which enforces a port's `Supports:` field where
  upstream's own CI only sees a warning. `blend2d` declares `Supports: !wasm32` and builds fine
  for wasm anyway, which is why the wasm job passes `--allow-unsupported`.
- **`VCPKG_CHAINLOAD_TOOLCHAIN_FILE` must be on the cmake command line**, not only in the
  triplet. A triplet governs how vcpkg builds ports; the consuming project is configured by its
  arguments alone. Getting this wrong builds wasm32 dependencies for a native x64 project, and
  the symptom is `find_package` reporting a package "not accepted, version: ... (32bit)" —
  which reads as a missing package.
- **`CMAKE_OSX_ARCHITECTURES` likewise.** Same split, same class of confusing error.
