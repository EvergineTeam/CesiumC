# Cesium C API

Minimal C wrapper for [Cesium Native](https://github.com/CesiumGS/cesium-native).

This repository exposes a plain C API (stable ABI boundary) on top of the Cesium Native C++ libraries.

## Status

Early stage. API surface is intentionally small and focused.

## Repository Layout

```text
include/        Public C headers (`cesium_native.h`)
src/            C/C++ implementation of the wrapper
tests/          Native tests for the C API
cesium-native/  Upstream Cesium Native source (submodule)
```

## Build (Windows)

Prerequisites:

- CMake 3.15+
- Visual Studio 2022 (C++ tools)
- vcpkg (`VCPKG_ROOT` set)

Typical flow:

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```