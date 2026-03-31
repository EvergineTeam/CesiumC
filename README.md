# Cesium C API

Plain C wrapper (stable ABI) for [Cesium Native](https://github.com/CesiumGS/cesium-native), designed for P/Invoke and FFI integration (C#, Rust, Python, etc.).

> **Status** — Early stage. Covers the core 3D Tiles streaming pipeline end-to-end but does not expose every cesium-native feature.

## Layout

```
include/cesium/   Public C headers (one per domain)
src/              C++ implementation
tests/            Native C API tests
example/          Minimal sample program
cesium-native/    Upstream submodule
```

## Build (Windows)

Requires CMake 3.15+, Visual Studio 2022+, and vcpkg (`VCPKG_ROOT` set).

```bash
git submodule update --init --recursive
build.bat
```

Or manually:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

Output: `build/bin/Release/CesiumNativeC.dll` + import lib.

## API Overview

Include `<cesium-native-api.h>` for everything, or pick individual headers.

| Header | Scope |
|--------|-------|
| `cesium_common.h` | Error handling, log callback, blittable math types (`CesiumVec3`, `CesiumMat4`, `CesiumCartographic`, `CesiumBoundingVolume`, …) |
| `cesium_geospatial.h` | Ellipsoid (WGS84), cartographic ↔ ECEF conversion, surface normals, globe rectangles, ENU transform |
| `cesium_gltf.h` | glTF/GLB reading, full read-only model introspection (meshes, primitives, materials, textures, images, nodes, accessors), zero-copy buffer access, GLB export |
| `cesium_tileset.h` | Async system, HTTP accessor, credit system, tileset creation (URL or Ion), per-frame view update, tile traversal, renderer resource callbacks, LOD/culling options |
| `cesium_raster_overlays.h` | Add/remove imagery overlays: Ion, URL template, TMS, WMS |
| `cesium_ion.h` | Ion connection, OAuth2 flow, asset & token listing |

## Key Concepts

**Error handling** — All fallible functions set a thread-local error string. Check with `cesium_get_last_error()` after any call that returns `0` / `NULL`. A log callback can be installed via `cesium_set_log_callback()`.

**Ownership** — Opaque handles returned by `_create()` functions must be freed with the matching `_destroy()`. Pointers returned by query functions (tile, model, view-update result) are **borrowed** and must not be freed by the caller.

**Frame loop** — A typical integration each frame:
1. `cesium_async_system_dispatch_main_thread_tasks()` — pump async completions
2. `cesium_tileset_update_view()` — submit camera(s), get tiles to render
3. Iterate `CesiumViewUpdateResult` for tiles data, bounding volumes, LOD status, etc.
4. `cesium_credit_system_start_next_frame()` — collect attribution strings

**Renderer callbacks** — Set via `cesium_tileset_externals_set_renderer_resource_callbacks()`. Six function pointers let you hook into the tile lifecycle: prepare GPU resources on load/main threads, free them, and attach/detach raster imagery. The `void*` user data round-trips through these callbacks.

**Blittable types** — All POD structs (`CesiumVec3`, `CesiumMat4`, `CesiumCartographic`, `CesiumBoundingVolume`, `CesiumMaterialData`, …) are layout-stable for direct marshalling in P/Invoke or FFI without manual packing.

## Example

See [`example/example.c`](example/example.c) for a complete program that loads Cesium World Terrain, runs one view update, and prints tile statistics. Link against `CesiumNativeC.lib` and ship `CesiumNativeC.dll` alongside the executable.

## Not Yet Exposed

This C binding is mostly focused on integration within render engines, therefore, some cesium-native capabilities are not intended to be covered. Right now, following features are not exposed by this C API:

- **Modules**: Cesium3DTilesContent, CesiumGltfContent (manipulation), CesiumGeometry, CesiumQuantizedMeshTerrain, CesiumVectorData, CesiumITwinClient
- **Raster overlays**: Bing Maps, Google Maps, WMTS, Azure Maps; per-overlay alpha/filter options
- **Ion client**: asset upload/delete, user profile
- **Geospatial**: Web Mercator / Geographic projections, `CartographicPolygon`, S2 cell utilities
- **Tileset**: custom `ITileExcluder`, content-loader factories, manual LOD override
- **By design**: template-heavy APIs, custom `IAssetAccessor`/`ITaskProcessor` implementations, spdlog access