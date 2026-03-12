# Cesium C API

Plain C wrapper (stable ABI boundary) for [Cesium Native](https://github.com/CesiumGS/cesium-native), designed for P/Invoke and FFI integration with languages such as C#, Rust, or Python.

## Status

Early stage. The API surface covers the core 3D Tiles streaming pipeline end-to-end but does not yet expose every cesium-native feature. See [Unsupported Capabilities](#unsupported-cesium-native-capabilities) for details.

## Repository Layout

```text
include/cesium/   Public C headers
src/              C++ glue implementation
tests/            Native tests for the C API
cesium-native/    Upstream Cesium Native (submodule)
build.bat         One-click build script (Windows)
```

## Build (Windows)

Prerequisites:

- CMake 3.15+
- Visual Studio 2022+ (C++ desktop workload)
- vcpkg (`VCPKG_ROOT` environment variable set)

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

The output is `build/bin/Release/CesiumNativeC.dll` (+ import lib).

## API Capabilities

### Error Handling (`cesium_common.h`)

- Thread-local last-error string: `cesium_get_last_error()` / `cesium_clear_last_error()`
- Configurable log callback: `cesium_set_log_callback()`
- All API functions that can fail set the error string; callers check after the call

### Blittable Types (`cesium_common.h`)

Plain-old-data structs safe for P/Invoke / FFI:

| Type | Description |
|------|-------------|
| `CesiumVec2` / `CesiumVec3` | 2D / 3D double vectors |
| `CesiumMat4` | Column-major 4×4 double matrix |
| `CesiumCartographic` | Longitude, latitude (radians), height (meters) |
| `CesiumGlobeRectangle` | West, south, east, north (radians) |
| `CesiumBoundingVolume` | Tagged union: region, oriented box, or sphere |

### Geospatial (`cesium_geospatial.h`)

- **Ellipsoid**: create custom, or use built-in WGS84 / unit-sphere singletons
- **Coordinate conversion**: cartographic ↔ cartesian, geodetic surface normal
- **Surface projection**: `scale_to_geodetic_surface`, `scale_to_geocentric_surface`
- **Convenience constructors**: `cesium_cartographic_from_degrees`, `cesium_globe_rectangle_from_degrees`
- **Globe rectangle queries**: width, height, center, contains-point
- **Globe transforms**: east-north-up to fixed-frame matrix

### glTF Reader (`cesium_gltf.h`)

- Create a `CesiumCGltfReader` and parse `.glb` / `.gltf` buffers
- Inspect parse result: errors, warnings, success
- Read-only model introspection: mesh count/names, material count, texture count, image count, node count, accessor count, buffer/buffer-view count, scene count, animation count, skin count

### 3D Tiles Streaming (`cesium_tileset.h`)

- **AsyncSystem**: thread-pool-backed async runtime; pump main-thread tasks with `dispatch_main_thread_tasks`
- **AssetAccessor**: libcurl-based HTTP accessor with configurable user-agent
- **CreditSystem**: per-frame credit collection and on-screen attribution strings
- **TilesetExternals**: bundles async system + accessor + credit system + renderer callbacks
- **TilesetOptions**: screen-space error, simultaneous loads, cache size, ancestor/sibling preloading, frustum/fog/occlusion culling, LOD transition period, load-error callback
- **ViewState**: create from perspective / orthographic parameters or raw view+projection matrices
- **Tileset**: create from URL or Cesium Ion asset ID; call `update_view` each frame
- **ViewUpdateResult**: tiles-to-render list, fading-out list, frame number, visited/culled/max-depth stats, load queue lengths
- **Tile**: geometric error, transform, load state, bounding volume, render content (glTF model pointer), render resources (user pointer), children traversal, LOD fade percentage
- **Renderer resource callbacks**: `prepareInLoadThread`, `prepareInMainThread`, `free`, `prepareRasterInLoadThread`, `prepareRasterInMainThread`, `freeRaster`

### Raster Overlays (`cesium_raster_overlays.h`)

- Get overlay collection from a tileset
- Add / remove overlays at runtime
- Supported overlay types:
  - **Cesium Ion** imagery (`cesium_ion_raster_overlay_create`)
  - **URL template** (`cesium_url_template_raster_overlay_create`)
  - **TMS** (`cesium_tile_map_service_raster_overlay_create`)
  - **WMS** (`cesium_web_map_service_raster_overlay_create`)

### Cesium Ion Client (`cesium_ion.h`)

- Create an Ion connection with access token
- OAuth2 authorization flow (`cesium_ion_connection_authorize`)
- List assets: ID, name, type
- List tokens: name, value

## Hello World Example

A minimal C program that creates a Cesium World Terrain tileset from Cesium Ion, runs one frame update, and prints the tiles selected for rendering.

```c
#include <cesium/cesium-native-api.h>
#include <math.h>
#include <stdio.h>

/*
 * Minimal Cesium Native C example:
 *   1. Set up the async / networking / credit infrastructure
 *   2. Open Cesium World Terrain (Ion asset 1) with your access token
 *   3. Pump the load loop until the root tile is available
 *   4. Perform a single view update and print what would be rendered
 */

/* Replace with your own Cesium Ion access token */
static const char* ION_ACCESS_TOKEN = "YOUR_CESIUM_ION_ACCESS_TOKEN";

/* Simple load-error callback – just prints the message */
static void on_load_error(int type, int httpStatus, const char* message,
                          void* userData) {
    (void)userData;
    fprintf(stderr, "[load error] type=%d http=%d: %s\n", type, httpStatus,
            message);
}

int main(void) {
    /* ---- 1. Create core systems ---- */
    CesiumAsyncSystem*    async    = cesium_async_system_create();
    CesiumAssetAccessor*  accessor = cesium_asset_accessor_create("HelloWorldApp/1.0");
    CesiumCreditSystem*   credits  = cesium_credit_system_create();

    CesiumTilesetExternals* externals =
        cesium_tileset_externals_create(async, accessor, credits);

    /* ---- 2. Configure tileset options ---- */
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_maximum_screen_space_error(options, 16.0);
    cesium_tileset_options_set_maximum_simultaneous_tile_loads(options, 10);
    cesium_tileset_options_set_load_error_callback(options, on_load_error, NULL);

    /* ---- 3. Open Cesium World Terrain (Ion asset ID 1) ---- */
    CesiumTileset* tileset = cesium_tileset_create_from_ion(
        externals, 1, ION_ACCESS_TOKEN, options, NULL);

    if (!tileset) {
        fprintf(stderr, "Failed to create tileset: %s\n", cesium_get_last_error());
        return 1;
    }

    /* ---- 4. Wait for the root tile to become available ---- */
    printf("Loading tileset...\n");
    for (int i = 0; i < 200; ++i) {           /* up to ~10 seconds */
        cesium_async_system_dispatch_main_thread_tasks(async);

        if (cesium_tileset_is_root_tile_available(tileset))
            break;

        /* Sleep 50 ms (platform-specific) */
#ifdef _WIN32
        Sleep(50);
#else
        usleep(50000);
#endif
    }

    float progress = cesium_tileset_compute_load_progress(tileset);
    printf("Load progress: %.1f%%\n", progress);

    if (!cesium_tileset_is_root_tile_available(tileset)) {
        fprintf(stderr, "Root tile did not become available in time.\n");
        /* Fall through to cleanup */
    }

    /* ---- 5. Build a camera view state ---- */
    /* Camera looking at New York City from ~1500 m altitude */
    const double lon = -74.006 * (3.14159265358979323846 / 180.0);
    const double lat =  40.7128 * (3.14159265358979323846 / 180.0);
    const double height = 1500.0;

    /* Convert camera position to ECEF */
    CesiumCartographic cameraCartographic = { lon, lat, height };
    const CesiumEllipsoid* wgs84 = cesium_ellipsoid_wgs84();
    CesiumVec3 cameraPosition =
        cesium_ellipsoid_cartographic_to_cartesian(wgs84, cameraCartographic);

    /* Camera direction: point toward Earth center (nadir) */
    CesiumVec3 direction = {
        -cameraPosition.x, -cameraPosition.y, -cameraPosition.z
    };
    /* Normalize */
    double len = sqrt(direction.x * direction.x +
                      direction.y * direction.y +
                      direction.z * direction.z);
    direction.x /= len;
    direction.y /= len;
    direction.z /= len;

    /* Approximate "up" from geodetic surface normal */
    CesiumVec3 up = cesium_ellipsoid_geodetic_surface_normal_cartographic(
        wgs84, cameraCartographic);

    CesiumVec2 viewportSize = { 1920.0, 1080.0 };

    CesiumViewState* viewState = cesium_view_state_create_perspective(
        cameraPosition, direction, up,
        /* viewport */ viewportSize,
        /* horizontal FOV (radians) */ 1.0472 /* ~60° */,
        /* vertical FOV (radians) */ 1.0472 /* ~60° */,
        /* ellipsoid */ wgs84);

    /* ---- 6. Update the tileset for this view ---- */
    const CesiumViewUpdateResult* result =
        cesium_tileset_update_view(tileset, &viewState, 1);

    int tileCount = cesium_view_update_result_get_tiles_to_render_count(result);
    printf("Tiles to render: %d\n", tileCount);

    /* Iterate over tiles selected for rendering */
    for (int i = 0; i < tileCount; ++i) {
        const CesiumTile* tile =
            cesium_view_update_result_get_tile_to_render(result, i);
        double error = cesium_tile_get_geometric_error(tile);
        int    state = cesium_tile_get_load_state(tile);
        printf("  tile[%d]: geometricError=%.2f  loadState=%d  hasContent=%d\n",
               i, error, state, cesium_tile_has_render_content(tile));
    }

    /* Print statistics */
    printf("Stats: visited=%u  culled=%u  maxDepth=%u\n",
           cesium_view_update_result_get_tiles_visited(result),
           cesium_view_update_result_get_tiles_culled(result),
           cesium_view_update_result_get_max_depth_visited(result));

    /* Print credits */
    cesium_credit_system_start_next_frame(credits);
    int creditCount =
        cesium_credit_system_get_credits_to_show_on_screen_count(credits);
    for (int i = 0; i < creditCount; ++i) {
        printf("Credit: %s\n",
               cesium_credit_system_get_credit_to_show_on_screen(credits, i));
    }

    /* ---- 7. Cleanup (reverse creation order) ---- */
    cesium_view_state_destroy(viewState);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    cesium_tileset_externals_destroy(externals);
    cesium_credit_system_destroy(credits);
    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);

    printf("Done.\n");
    return 0;
}
```

### Building the example

Link against `CesiumNativeC.lib` (Windows) and make sure `CesiumNativeC.dll` is in the executable's directory at runtime.

## Unsupported Cesium-Native Capabilities

The following cesium-native modules and features are **not** exposed through this C API:

### Entire modules not wrapped

| Module | What it provides |
|--------|-----------------|
| **Cesium3DTiles** | Low-level 3D Tiles data structures (tile JSON schema, content specs, properties) |
| **Cesium3DTilesContent** | Tile content loading/conversion (B3DM, I3DM, PNTS, CMPT handlers, implicit tiling) |
| **Cesium3DTilesReader / Writer** | 3D Tiles JSON deserialization/serialization, subtree files, extension support |
| **CesiumGltfContent** | glTF manipulation (mesh creation, material editing, buffer operations) |
| **CesiumGltfWriter** | glTF serialization/export |
| **CesiumGeometry** | Intersection tests, bounding volume operations, culling volumes, spatial indexing (quad/octrees) |
| **CesiumQuantizedMeshTerrain** | Quantized-mesh terrain format loading |
| **CesiumVectorData** | GeoJSON loading, vector rasterization, vector styling |
| **CesiumITwinClient** | Bentley iTwin platform integration |
| **CesiumJsonReader / Writer** | Low-level typed JSON serialization |
| **CesiumUtility** | URI utilities, extended logging, spdlog integration |

### Partial module gaps

| Area | What is missing |
|------|----------------|
| **glTF model** | Write/modify access (read-only today); per-primitive accessor data; Draco compression control |
| **Tileset** | Custom `ITileExcluder` implementations; custom content-loader factories; manual LOD override |
| **Raster overlays** | Bing Maps, Google Maps, WMTS, and Azure Maps overlay types; per-overlay options (alpha, cut, filter) |
| **Ion client** | Asset upload/creation/deletion; user profile queries |
| **Credit system** | HTML-formatted credits; popup vs. on-screen distinction |
| **Geospatial** | Web Mercator / Geographic projection classes; `CartographicPolygon`; `S2CellID` utilities |
| **Async** | Manual future/promise chaining from C; cancellation tokens |

### By design

Some features are intentionally omitted because they require C++ constructs that cannot be safely expressed through a C ABI:

- **Template-heavy APIs** (typed property access, visitor patterns)
- **Custom interface implementations** beyond `IPrepareRendererResources` (e.g., `IAssetAccessor`, `ITaskProcessor`)
- **Extensible reader/writer options** with arbitrary extension registrations
- **Direct spdlog logger access**