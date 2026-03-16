#include <cesium-native-api.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/*
 * Minimal Cesium Native C example:
 *   1. Set up the async / networking / credit infrastructure
 *   2. Open Cesium World Terrain (Ion asset 1) with your access token
 *   3. Pump the load loop until the root tile is available
 *   4. Perform a single view update and print what would be rendered
 */

/* Simple load-error callback - just prints the message */
static void on_load_error(void* userData, const char* message) {
    (void)userData;
    fprintf(stderr, "[load error] %s\n", message);
}

int main(void) {
    /* ---- Read Cesium Ion access token from console ---- */
    char ionAccessToken[2048];
    printf("Enter your Cesium Ion access token: ");
    fflush(stdout);
    if (!fgets(ionAccessToken, sizeof(ionAccessToken), stdin)) {
        fprintf(stderr, "Failed to read access token.\n");
        return 1;
    }
    /* Strip trailing newline */
    size_t tokenLen = strlen(ionAccessToken);
    if (tokenLen > 0 && ionAccessToken[tokenLen - 1] == '\n')
        ionAccessToken[--tokenLen] = '\0';
    if (tokenLen > 0 && ionAccessToken[tokenLen - 1] == '\r')
        ionAccessToken[--tokenLen] = '\0';
    if (tokenLen == 0) {
        fprintf(stderr, "Access token cannot be empty.\n");
        return 1;
    }

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
        externals, 1, ionAccessToken, options, NULL);

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
        /* horizontal FOV (radians) */ 1.0472 /* ~60 deg */,
        /* vertical FOV (radians) */ 1.0472 /* ~60 deg */,
        /* ellipsoid */ wgs84);

    /* ---- 6. Update the tileset for this view ---- */
    const CesiumViewState* viewStates[] = { viewState };
    const CesiumViewUpdateResult* result =
        cesium_tileset_update_view(tileset, viewStates, 1, 0.016f);

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
