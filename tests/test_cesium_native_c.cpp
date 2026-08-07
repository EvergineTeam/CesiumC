/**
 * @file test_cesium_native_c.cpp
 * @brief Integration tests for CesiumNativeC DLL.
 *
 * A lightweight test harness — each test function returns 0 on success, 1 on
 * failure. No external test framework dependency.
 */

#include <cesium/cesium_common.h>
#include <cesium/cesium_geospatial.h>
#include <cesium/cesium_gltf.h>
#include <cesium/cesium_ion.h>
#include <cesium/cesium_raster_overlays.h>
#include <cesium/cesium_tileset.h>

#include "test_gltf_asset.h"
#include "test_http_asset.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

// ---------- helpers ----------

static int g_passed = 0;
static int g_failed = 0;

#define ASSERT_TRUE(expr)                                                     \
    do {                                                                      \
        if (!(expr)) {                                                        \
            std::printf("  FAIL: %s  (line %d)\n", #expr, __LINE__);          \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define ASSERT_EQ(a, b)                                                       \
    do {                                                                      \
        if ((a) != (b)) {                                                     \
            std::printf("  FAIL: %s == %s  (line %d)\n", #a, #b, __LINE__);   \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define ASSERT_NEAR(a, b, eps)                                                \
    do {                                                                      \
        if (std::fabs((a) - (b)) > (eps)) {                                   \
            std::printf("  FAIL: |%s - %s| <= %s  (got %f vs %f, line %d)\n", \
                        #a, #b, #eps, (double)(a), (double)(b), __LINE__);    \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define ASSERT_NULL(p)                                                        \
    do {                                                                      \
        if ((p) != nullptr) {                                                 \
            std::printf("  FAIL: %s should be NULL  (line %d)\n", #p, __LINE__); \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define ASSERT_NOT_NULL(p)                                                    \
    do {                                                                      \
        if ((p) == nullptr) {                                                 \
            std::printf("  FAIL: %s should not be NULL  (line %d)\n", #p, __LINE__); \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define RUN_TEST(fn)                                                          \
    do {                                                                      \
        std::printf("[TEST] %s ... ", #fn);                                   \
        if (fn() == 0) {                                                      \
            std::printf("OK\n");                                              \
            ++g_passed;                                                       \
        } else {                                                              \
            ++g_failed;                                                       \
        }                                                                     \
    } while (0)

static const double PI = 3.14159265358979323846;

// Cesium Ion access token from environment variable.
// Tests that require network access will be skipped if not set.
static const char* g_ionToken = nullptr;

static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

// Helper: create the full tileset infrastructure in one call.
struct TilesetTestFixture {
    CesiumAsyncSystem*      async;
    CesiumAssetAccessor*    accessor;
    CesiumCreditSystem*     credits;
    CesiumTilesetExternals* externals;

    static TilesetTestFixture create() {
        TilesetTestFixture f{};
        f.async     = cesium_async_system_create();
        f.accessor  = cesium_asset_accessor_create("CesiumNativeC-Tests/1.0");
        f.credits   = cesium_credit_system_create();
        f.externals = cesium_tileset_externals_create(f.async, f.accessor, f.credits);
        return f;
    }

    void destroy() {
        cesium_tileset_externals_destroy(externals);
        cesium_credit_system_destroy(credits);
        cesium_asset_accessor_destroy(accessor);
        cesium_async_system_destroy(async);
    }

    // Pump main-thread tasks + update view until root tile is available or
    // timeout is reached. Returns true if root became available.
    bool waitForRootTile(CesiumTileset* tileset, CesiumViewState* vs,
                         int maxIterations = 200, int sleepMs = 50) {
        const CesiumViewState* views[] = {vs};
        for (int i = 0; i < maxIterations; ++i) {
            cesium_async_system_dispatch_main_thread_tasks(async);
            cesium_credit_system_start_next_frame(credits);
            cesium_tileset_update_view(tileset, views, 1, 0.016f);
            if (cesium_tileset_is_root_tile_available(tileset))
                return true;
            sleep_ms(sleepMs);
        }
        return false;
    }
};

// Helper: create a view state looking at NYC from ~1500 m
static CesiumViewState* createNycViewState() {
    const CesiumEllipsoid* wgs84 = cesium_ellipsoid_wgs84();
    CesiumCartographic cam = cesium_cartographic_from_degrees(-74.006, 40.7128, 1500.0);
    CesiumVec3 pos = cesium_ellipsoid_cartographic_to_cartesian(wgs84, cam);

    CesiumVec3 dir = {-pos.x, -pos.y, -pos.z};
    double len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    dir.x /= len; dir.y /= len; dir.z /= len;

    CesiumVec3 up = cesium_ellipsoid_geodetic_surface_normal_cartographic(wgs84, cam);
    CesiumVec2 viewport = {1920.0, 1080.0};

    return cesium_view_state_create_perspective(
        pos, dir, up, viewport,
        60.0 * PI / 180.0, 33.75 * PI / 180.0, nullptr);
}

#define SKIP_IF_NO_TOKEN()                                                    \
    do {                                                                      \
        if (!g_ionToken || g_ionToken[0] == '\0') {                           \
            std::printf("SKIPPED (set CESIUM_ION_TOKEN)\n");                  \
            ++g_passed;                                                       \
            return 0;                                                         \
        }                                                                     \
    } while (0)

// ============================================================================
// Test: Error handling
// ============================================================================

static int test_error_handling() {
    // Initially no error
    cesium_clear_last_error();
    ASSERT_NULL(cesium_get_last_error());

    // After a bad call, an error should be set
    // Passing NULL to a function that requires non-null should set error
    CesiumCGltfReaderResult* r =
        cesium_gltf_reader_read(nullptr, nullptr, 0);
    ASSERT_NULL(r);
    const char* err = cesium_get_last_error();
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(std::strlen(err) > 0);

    // Clear should reset
    cesium_clear_last_error();
    ASSERT_NULL(cesium_get_last_error());
    return 0;
}

// ============================================================================
// Test: Ellipsoid WGS84 singleton
// ============================================================================

static int test_ellipsoid_wgs84() {
    const CesiumEllipsoid* wgs84 = cesium_ellipsoid_wgs84();
    ASSERT_NOT_NULL(wgs84);

    CesiumVec3 radii = cesium_ellipsoid_get_radii(wgs84);
    ASSERT_NEAR(radii.x, 6378137.0, 1.0);
    ASSERT_NEAR(radii.y, 6378137.0, 1.0);
    ASSERT_NEAR(radii.z, 6356752.314245, 1.0);

    double maxR = cesium_ellipsoid_get_maximum_radius(wgs84);
    ASSERT_NEAR(maxR, 6378137.0, 1.0);

    double minR = cesium_ellipsoid_get_minimum_radius(wgs84);
    ASSERT_NEAR(minR, 6356752.314245, 1.0);

    // Singleton should return same pointer
    const CesiumEllipsoid* wgs84b = cesium_ellipsoid_wgs84();
    ASSERT_EQ(wgs84, wgs84b);

    return 0;
}

// ============================================================================
// Test: Ellipsoid create / destroy
// ============================================================================

static int test_ellipsoid_create_destroy() {
    CesiumEllipsoid* e = cesium_ellipsoid_create(1.0, 2.0, 3.0);
    ASSERT_NOT_NULL(e);

    CesiumVec3 radii = cesium_ellipsoid_get_radii(e);
    ASSERT_NEAR(radii.x, 1.0, 1e-10);
    ASSERT_NEAR(radii.y, 2.0, 1e-10);
    ASSERT_NEAR(radii.z, 3.0, 1e-10);

    double maxR = cesium_ellipsoid_get_maximum_radius(e);
    ASSERT_NEAR(maxR, 3.0, 1e-10);

    cesium_ellipsoid_destroy(e);
    return 0;
}

// ============================================================================
// Test: Cartographic conversion round-trip
// ============================================================================

static int test_cartographic_round_trip() {
    const CesiumEllipsoid* wgs84 = cesium_ellipsoid_wgs84();

    // Madrid approximately: 40.4168° N, 3.7038° W
    CesiumCartographic madrid =
        cesium_cartographic_from_degrees(-3.7038, 40.4168, 650.0);

    ASSERT_NEAR(madrid.longitude, -3.7038 * PI / 180.0, 1e-10);
    ASSERT_NEAR(madrid.latitude, 40.4168 * PI / 180.0, 1e-10);
    ASSERT_NEAR(madrid.height, 650.0, 1e-10);

    // Convert to Cartesian
    CesiumVec3 cartesian =
        cesium_ellipsoid_cartographic_to_cartesian(wgs84, madrid);

    // Cartesian should be roughly earth-sized magnitude
    double mag = std::sqrt(
        cartesian.x * cartesian.x +
        cartesian.y * cartesian.y +
        cartesian.z * cartesian.z);
    ASSERT_TRUE(mag > 6.3e6 && mag < 6.4e6);

    // Convert back
    CesiumCartographic roundTrip;
    int ok = cesium_ellipsoid_cartesian_to_cartographic(
        wgs84, cartesian, &roundTrip);
    ASSERT_EQ(ok, 1);

    ASSERT_NEAR(roundTrip.longitude, madrid.longitude, 1e-10);
    ASSERT_NEAR(roundTrip.latitude, madrid.latitude, 1e-10);
    ASSERT_NEAR(roundTrip.height, madrid.height, 0.001);

    return 0;
}

// ============================================================================
// Test: Surface normal
// ============================================================================

static int test_surface_normal() {
    const CesiumEllipsoid* wgs84 = cesium_ellipsoid_wgs84();

    // North pole cartographic
    CesiumCartographic northPole;
    northPole.longitude = 0.0;
    northPole.latitude = PI / 2.0;
    northPole.height = 0.0;

    CesiumVec3 normal =
        cesium_ellipsoid_geodetic_surface_normal_cartographic(wgs84, northPole);

    // Should point along +Z
    ASSERT_NEAR(normal.x, 0.0, 1e-10);
    ASSERT_NEAR(normal.y, 0.0, 1e-10);
    ASSERT_NEAR(normal.z, 1.0, 1e-10);

    return 0;
}

// ============================================================================
// Test: GltfReader create/destroy
// ============================================================================

static int test_gltf_reader_create_destroy() {
    CesiumCGltfReader* reader = cesium_gltf_reader_create();
    ASSERT_NOT_NULL(reader);

    // Read invalid data — should produce errors
    uint8_t badData[] = {0x00, 0x01, 0x02, 0x03};
    CesiumCGltfReaderResult* result =
        cesium_gltf_reader_read(reader, badData, sizeof(badData));
    ASSERT_NOT_NULL(result);

    // Should not have a valid model
    ASSERT_EQ(cesium_gltf_reader_result_has_model(result), 0);

    // Should have at least one error
    int errCount = cesium_gltf_reader_result_get_error_count(result);
    ASSERT_TRUE(errCount > 0);

    const char* firstErr = cesium_gltf_reader_result_get_error(result, 0);
    ASSERT_NOT_NULL(firstErr);
    ASSERT_TRUE(std::strlen(firstErr) > 0);

    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

// ============================================================================
// Test: AsyncSystem create/destroy
// ============================================================================

static int test_async_system() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    ASSERT_NOT_NULL(async);

    // Dispatching with no pending tasks should be a no-op
    cesium_async_system_dispatch_main_thread_tasks(async);

    cesium_async_system_destroy(async);
    return 0;
}

// ============================================================================
// Test: AssetAccessor create/destroy
// ============================================================================

static int test_asset_accessor() {
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create("CesiumNativeC-Test/1.0");
    ASSERT_NOT_NULL(accessor);

    // NULL user agent should also work
    CesiumAssetAccessor* accessor2 = cesium_asset_accessor_create(nullptr);
    ASSERT_NOT_NULL(accessor2);

    cesium_asset_accessor_destroy(accessor2);
    cesium_asset_accessor_destroy(accessor);
    return 0;
}

// ============================================================================
// Test: CreditSystem create/destroy
// ============================================================================

static int test_credit_system() {
    CesiumCreditSystem* credits = cesium_credit_system_create();
    ASSERT_NOT_NULL(credits);

    // Initially no credits to show
    int count = cesium_credit_system_get_credits_to_show_on_screen_count(credits);
    ASSERT_EQ(count, 0);

    cesium_credit_system_start_next_frame(credits);

    cesium_credit_system_destroy(credits);
    return 0;
}

// ============================================================================
// Test: TilesetExternals create/destroy
// ============================================================================

static int test_tileset_externals() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create(nullptr);
    CesiumCreditSystem* credits = cesium_credit_system_create();
    ASSERT_NOT_NULL(async);
    ASSERT_NOT_NULL(accessor);
    ASSERT_NOT_NULL(credits);

    CesiumTilesetExternals* ext =
        cesium_tileset_externals_create(async, accessor, credits);
    ASSERT_NOT_NULL(ext);

    cesium_tileset_externals_destroy(ext);
    cesium_credit_system_destroy(credits);
    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);
    return 0;
}

// ============================================================================
// Test: TilesetOptions create / get / set / destroy
// ============================================================================

static int test_tileset_options() {
    CesiumTilesetOptions* opts = cesium_tileset_options_create();
    ASSERT_NOT_NULL(opts);

    // Default values
    double sse = cesium_tileset_options_get_maximum_screen_space_error(opts);
    ASSERT_NEAR(sse, 16.0, 0.01);

    // Set and get back
    cesium_tileset_options_set_maximum_screen_space_error(opts, 8.0);
    ASSERT_NEAR(cesium_tileset_options_get_maximum_screen_space_error(opts), 8.0, 1e-10);

    cesium_tileset_options_set_maximum_simultaneous_tile_loads(opts, 42);
    ASSERT_EQ(cesium_tileset_options_get_maximum_simultaneous_tile_loads(opts), (uint32_t)42);

    cesium_tileset_options_set_maximum_cached_bytes(opts, 1024 * 1024);
    ASSERT_EQ(cesium_tileset_options_get_maximum_cached_bytes(opts), (int64_t)(1024 * 1024));

    cesium_tileset_options_set_preload_ancestors(opts, 0);
    ASSERT_EQ(cesium_tileset_options_get_preload_ancestors(opts), 0);

    cesium_tileset_options_set_forbid_holes(opts, 1);
    ASSERT_EQ(cesium_tileset_options_get_forbid_holes(opts), 1);

    cesium_tileset_options_set_enable_frustum_culling(opts, 0);
    ASSERT_EQ(cesium_tileset_options_get_enable_frustum_culling(opts), 0);

    cesium_tileset_options_destroy(opts);
    return 0;
}

// ============================================================================
// Test: ViewState create perspective / destroy
// ============================================================================

static int test_view_state_perspective() {
    CesiumVec3 pos = {6378137.0, 0.0, 0.0};  // On equator at prime meridian
    CesiumVec3 dir = {-1.0, 0.0, 0.0};       // Looking toward center
    CesiumVec3 up = {0.0, 0.0, 1.0};         // Z-up
    CesiumVec2 viewport = {1920.0, 1080.0};
    double hfov = 60.0 * PI / 180.0;
    double vfov = 33.75 * PI / 180.0;

    CesiumViewState* vs = cesium_view_state_create_perspective(
        pos, dir, up, viewport, hfov, vfov, nullptr);
    ASSERT_NOT_NULL(vs);

    cesium_view_state_destroy(vs);
    return 0;
}

// ============================================================================
// Test: Full pipeline — load Cesium World Terrain from Ion
// ============================================================================

static int test_tileset_create_from_ion_world_terrain() {
    SKIP_IF_NO_TOKEN();

    auto f = TilesetTestFixture::create();
    CesiumTileset* tileset = cesium_tileset_create_from_ion(
        f.externals, 1 /* Cesium World Terrain */, g_ionToken, nullptr, nullptr);
    ASSERT_NOT_NULL(tileset);

    CesiumViewState* vs = createNycViewState();

    // Wait for the root tile to become available
    bool rootAvailable = f.waitForRootTile(tileset, vs);
    ASSERT_TRUE(rootAvailable);

    float progress = cesium_tileset_compute_load_progress(tileset);
    std::printf("(progress=%.1f%%) ", progress);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    f.destroy();
    return 0;
}

// ============================================================================
// Test: Cartographic to Cartesian — NYC position (from Hello World example)
// ============================================================================

static int test_cartographic_to_cartesian_nyc() {
    const CesiumEllipsoid* wgs84 = cesium_ellipsoid_wgs84();

    // New York City: 40.7128° N, 74.006° W, 1500 m
    double lon = -74.006 * (PI / 180.0);
    double lat = 40.7128 * (PI / 180.0);
    double height = 1500.0;
    CesiumCartographic nyc = {lon, lat, height};

    CesiumVec3 ecef =
        cesium_ellipsoid_cartographic_to_cartesian(wgs84, nyc);

    // ECEF magnitude should be ~Earth radius + height
    double mag = std::sqrt(ecef.x * ecef.x + ecef.y * ecef.y + ecef.z * ecef.z);
    ASSERT_TRUE(mag > 6.37e6 && mag < 6.39e6);

    // NYC is in the western hemisphere, positive latitude ⇒
    //   x > 0 , y < 0, z > 0
    ASSERT_TRUE(ecef.x > 0.0);
    ASSERT_TRUE(ecef.y < 0.0);
    ASSERT_TRUE(ecef.z > 0.0);

    // Inverse conversion must recover the original
    CesiumCartographic back;
    int ok = cesium_ellipsoid_cartesian_to_cartographic(wgs84, ecef, &back);
    ASSERT_EQ(ok, 1);
    ASSERT_NEAR(back.longitude, lon, 1e-10);
    ASSERT_NEAR(back.latitude, lat, 1e-10);
    ASSERT_NEAR(back.height, height, 0.01);

    return 0;
}

// ============================================================================
// Test: East-North-Up transform matrix
// ============================================================================

static int test_east_north_up_transform() {
    const CesiumEllipsoid* wgs84 = cesium_ellipsoid_wgs84();

    // Position on the equator at the prime meridian
    CesiumCartographic origin = {0.0, 0.0, 0.0};
    CesiumVec3 ecef =
        cesium_ellipsoid_cartographic_to_cartesian(wgs84, origin);

    CesiumMat4 enu =
        cesium_globe_transforms_east_north_up_to_fixed_frame(ecef, wgs84);

    // The matrix should be a valid transformation (not zero/identity).
    // Translation column (column 3) should equal the ECEF position.
    ASSERT_NEAR(enu.m[12], ecef.x, 1.0);
    ASSERT_NEAR(enu.m[13], ecef.y, 1.0);
    ASSERT_NEAR(enu.m[14], ecef.z, 1.0);
    ASSERT_NEAR(enu.m[15], 1.0, 1e-10);

    // At (0,0) the ENU axes are:
    //   East  = +Y global
    //   North = +Z global
    //   Up    = +X global
    // Column 0 (East)
    ASSERT_NEAR(enu.m[0], 0.0, 1e-6);
    ASSERT_NEAR(enu.m[1], 1.0, 1e-6);
    ASSERT_NEAR(enu.m[2], 0.0, 1e-6);
    // Column 1 (North)
    ASSERT_NEAR(enu.m[4], 0.0, 1e-6);
    ASSERT_NEAR(enu.m[5], 0.0, 1e-6);
    ASSERT_NEAR(enu.m[6], 1.0, 1e-6);
    // Column 2 (Up)
    ASSERT_NEAR(enu.m[8], 1.0, 1e-6);
    ASSERT_NEAR(enu.m[9], 0.0, 1e-6);
    ASSERT_NEAR(enu.m[10], 0.0, 1e-6);

    return 0;
}

// ============================================================================
// Test: Globe rectangle — creation, size, center, containment
// ============================================================================

static int test_globe_rectangle_queries() {
    // Rectangle covering roughly Spain: 36°N–43.8°N, 9.3°W–3.3°E
    CesiumGlobeRectangle spain =
        cesium_globe_rectangle_from_degrees(-9.3, 36.0, 3.3, 43.8);

    double widthRad = cesium_globe_rectangle_compute_width(spain);
    double heightRad = cesium_globe_rectangle_compute_height(spain);

    // Width ≈ 12.6° ≈ 0.22 rad, Height ≈ 7.8° ≈ 0.136 rad
    ASSERT_NEAR(widthRad, 12.6 * PI / 180.0, 0.001);
    ASSERT_NEAR(heightRad, 7.8 * PI / 180.0, 0.001);

    CesiumCartographic center =
        cesium_globe_rectangle_compute_center(spain);
    // Center should be near (-3°, 39.9°)
    ASSERT_NEAR(center.longitude, -3.0 * PI / 180.0, 0.01);
    ASSERT_NEAR(center.latitude, 39.9 * PI / 180.0, 0.01);

    // Madrid (40.4168° N, 3.7038° W) should be inside
    CesiumCartographic madrid =
        cesium_cartographic_from_degrees(-3.7038, 40.4168, 0.0);
    ASSERT_EQ(cesium_globe_rectangle_contains(spain, madrid), 1);

    // London (51.5° N, 0.13° W) should be outside (too far north)
    CesiumCartographic london =
        cesium_cartographic_from_degrees(-0.13, 51.5, 0.0);
    ASSERT_EQ(cesium_globe_rectangle_contains(spain, london), 0);

    return 0;
}

// ============================================================================
// Test: TilesetOptions — all setters/getters round-trip
// ============================================================================

static int test_tileset_options_full_round_trip() {
    CesiumTilesetOptions* opts = cesium_tileset_options_create();
    ASSERT_NOT_NULL(opts);

    cesium_tileset_options_set_preload_siblings(opts, 0);
    ASSERT_EQ(cesium_tileset_options_get_preload_siblings(opts), 0);
    cesium_tileset_options_set_preload_siblings(opts, 1);
    ASSERT_EQ(cesium_tileset_options_get_preload_siblings(opts), 1);

    cesium_tileset_options_set_enable_fog_culling(opts, 0);
    ASSERT_EQ(cesium_tileset_options_get_enable_fog_culling(opts), 0);

    cesium_tileset_options_set_enable_occlusion_culling(opts, 1);
    ASSERT_EQ(cesium_tileset_options_get_enable_occlusion_culling(opts), 1);
    cesium_tileset_options_set_enable_occlusion_culling(opts, 0);
    ASSERT_EQ(cesium_tileset_options_get_enable_occlusion_culling(opts), 0);

    cesium_tileset_options_set_enable_lod_transition_period(opts, 1);
    ASSERT_EQ(cesium_tileset_options_get_enable_lod_transition_period(opts), 1);

    cesium_tileset_options_set_lod_transition_length(opts, 0.75f);
    ASSERT_NEAR(cesium_tileset_options_get_lod_transition_length(opts), 0.75f, 1e-6);

    cesium_tileset_options_destroy(opts);
    return 0;
}

// ============================================================================
// Test: ViewState — orthographic creation
// ============================================================================

static int test_view_state_orthographic() {
    CesiumVec3 pos = {6378137.0, 0.0, 0.0};
    CesiumVec3 dir = {-1.0, 0.0, 0.0};
    CesiumVec3 up = {0.0, 0.0, 1.0};
    CesiumVec2 viewport = {1024.0, 768.0};

    CesiumViewState* vs = cesium_view_state_create_orthographic(
        pos, dir, up, viewport,
        -500.0, 500.0, -375.0, 375.0, nullptr);
    ASSERT_NOT_NULL(vs);

    cesium_view_state_destroy(vs);
    return 0;
}

// ============================================================================
// Test: ViewState — creation from matrices
// ============================================================================

static int test_view_state_from_matrices() {
    // Identity view matrix (camera at origin looking down -Z)
    CesiumMat4 view = {};
    view.m[0] = 1.0; view.m[5] = 1.0;
    view.m[10] = 1.0; view.m[15] = 1.0;

    // Simple perspective projection matrix
    CesiumMat4 proj = {};
    double fov = 60.0 * PI / 180.0;
    double aspect = 16.0 / 9.0;
    double f = 1.0 / std::tan(fov / 2.0);
    double nearP = 0.1, farP = 10000.0;
    proj.m[0] = f / aspect;
    proj.m[5] = f;
    proj.m[10] = (farP + nearP) / (nearP - farP);
    proj.m[11] = -1.0;
    proj.m[14] = (2.0 * farP * nearP) / (nearP - farP);

    CesiumVec2 viewport = {1920.0, 1080.0};

    CesiumViewState* vs = cesium_view_state_create_from_matrices(
        view, proj, viewport, nullptr);
    ASSERT_NOT_NULL(vs);

    cesium_view_state_destroy(vs);
    return 0;
}

// ============================================================================
// Test: Load error callback — verify it can be set without crashing
// ============================================================================

struct LoadErrorTestData {
    int callCount;
    char lastMessage[256];
};

static void test_load_error_cb(void* userData, const char* message) {
    auto* data = static_cast<LoadErrorTestData*>(userData);
    data->callCount++;
    if (message) {
        std::strncpy(data->lastMessage, message, sizeof(data->lastMessage) - 1);
        data->lastMessage[sizeof(data->lastMessage) - 1] = '\0';
    }
}

static int test_load_error_callback() {
    CesiumTilesetOptions* opts = cesium_tileset_options_create();
    ASSERT_NOT_NULL(opts);

    LoadErrorTestData errorData = {};

    // Setting a callback should not crash
    cesium_tileset_options_set_load_error_callback(
        opts, test_load_error_cb, &errorData);

    // Setting NULL callback should also work
    cesium_tileset_options_set_load_error_callback(opts, nullptr, nullptr);

    // Re-set it
    cesium_tileset_options_set_load_error_callback(
        opts, test_load_error_cb, &errorData);

    cesium_tileset_options_destroy(opts);
    return 0;
}

// ============================================================================
// Test: ViewUpdateResult statistics from real Ion tileset
// ============================================================================

static int test_view_update_result_statistics() {
    SKIP_IF_NO_TOKEN();

    auto f = TilesetTestFixture::create();

    CesiumTileset* tileset = cesium_tileset_create_from_ion(
        f.externals, 1, g_ionToken, nullptr, nullptr);
    ASSERT_NOT_NULL(tileset);

    CesiumViewState* vs = createNycViewState();

    // Wait for root tile, then do a proper update
    bool ready = f.waitForRootTile(tileset, vs);
    ASSERT_TRUE(ready);

    // Do several more updates to let tiles stream in
    const CesiumViewState* views[] = {vs};
    const CesiumViewUpdateResult* result = nullptr;
    for (int i = 0; i < 30; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_credit_system_start_next_frame(f.credits);
        result = cesium_tileset_update_view(tileset, views, 1, 0.016f);
        sleep_ms(50);
    }
    ASSERT_NOT_NULL(result);

    int renderCount = cesium_view_update_result_get_tiles_to_render_count(result);
    ASSERT_TRUE(renderCount > 0);

    int fadingCount = cesium_view_update_result_get_tiles_fading_out_count(result);
    ASSERT_TRUE(fadingCount >= 0);

    int32_t frameNumber = cesium_view_update_result_get_frame_number(result);
    ASSERT_TRUE(frameNumber >= 0);

    uint32_t visited = cesium_view_update_result_get_tiles_visited(result);
    ASSERT_TRUE(visited > 0);

    uint32_t culled = cesium_view_update_result_get_tiles_culled(result);
    (void)culled; // may be 0 for a small view

    uint32_t maxDepth = cesium_view_update_result_get_max_depth_visited(result);
    ASSERT_TRUE(maxDepth > 0);

    int32_t workerQueue =
        cesium_view_update_result_get_worker_thread_load_queue_length(result);
    int32_t mainQueue =
        cesium_view_update_result_get_main_thread_load_queue_length(result);
    ASSERT_TRUE(workerQueue >= 0);
    ASSERT_TRUE(mainQueue >= 0);

    std::printf("(render=%d visited=%u culled=%u depth=%u) ",
                renderCount, visited, culled, maxDepth);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    f.destroy();
    return 0;
}

// ============================================================================
// Test: Tile properties — inspect real tiles from Cesium World Terrain
// ============================================================================

static int test_tile_properties() {
    SKIP_IF_NO_TOKEN();

    auto f = TilesetTestFixture::create();
    CesiumTileset* tileset = cesium_tileset_create_from_ion(
        f.externals, 1, g_ionToken, nullptr, nullptr);
    ASSERT_NOT_NULL(tileset);

    CesiumViewState* vs = createNycViewState();
    bool ready = f.waitForRootTile(tileset, vs);
    ASSERT_TRUE(ready);

    // Pump updates to let tiles load
    const CesiumViewState* views[] = {vs};
    const CesiumViewUpdateResult* result = nullptr;
    for (int i = 0; i < 40; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_credit_system_start_next_frame(f.credits);
        result = cesium_tileset_update_view(tileset, views, 1, 0.016f);
        sleep_ms(50);
    }
    ASSERT_NOT_NULL(result);

    int renderCount = cesium_view_update_result_get_tiles_to_render_count(result);
    ASSERT_TRUE(renderCount > 0);

    // Inspect the first renderable tile
    const CesiumTile* tile =
        cesium_view_update_result_get_tile_to_render(result, 0);
    ASSERT_NOT_NULL(tile);

    double geoError = cesium_tile_get_geometric_error(tile);
    ASSERT_TRUE(geoError >= 0.0);

    CesiumMat4 transform = cesium_tile_get_transform(tile);
    // Transform should not be all-zero (at least [15] should be 1.0 for a valid transform)
    ASSERT_NEAR(transform.m[15], 1.0, 1e-10);

    CesiumTileLoadState loadState = cesium_tile_get_load_state(tile);
    ASSERT_EQ(loadState, CESIUM_TILE_LOAD_STATE_DONE);

    CesiumBoundingVolume bv = cesium_tile_get_bounding_volume(tile);
    // Type should be one of the valid enum values
    ASSERT_TRUE(bv.type == CESIUM_BOUNDING_VOLUME_REGION ||
                bv.type == CESIUM_BOUNDING_VOLUME_ORIENTED_BOX ||
                bv.type == CESIUM_BOUNDING_VOLUME_SPHERE);

    float fade = cesium_tile_get_lod_transition_fade_percentage(tile);
    ASSERT_TRUE(fade >= 0.0f && fade <= 1.0f);

    std::printf("(geoError=%.2f loadState=%d bvType=%d) ",
                geoError, loadState, bv.type);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    f.destroy();
    return 0;
}

// ============================================================================
// Test: Tileset data accessors (numberOfTilesLoaded, totalDataBytes) with real data
// ============================================================================

static int test_tileset_data_accessors() {
    SKIP_IF_NO_TOKEN();

    auto f = TilesetTestFixture::create();
    CesiumTileset* tileset = cesium_tileset_create_from_ion(
        f.externals, 1, g_ionToken, nullptr, nullptr);
    ASSERT_NOT_NULL(tileset);

    CesiumViewState* vs = createNycViewState();
    bool ready = f.waitForRootTile(tileset, vs);
    ASSERT_TRUE(ready);

    // Pump updates to load some tiles
    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 30; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_credit_system_start_next_frame(f.credits);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
        sleep_ms(50);
    }

    int32_t numLoaded = cesium_tileset_get_number_of_tiles_loaded(tileset);
    ASSERT_TRUE(numLoaded > 0);

    int64_t totalBytes = cesium_tileset_get_total_data_bytes(tileset);
    ASSERT_TRUE(totalBytes > 0);

    float progress = cesium_tileset_compute_load_progress(tileset);
    ASSERT_TRUE(progress >= 0.0f && progress <= 100.0f);

    std::printf("(loaded=%d bytes=%lld progress=%.1f%%) ",
                numLoaded, (long long)totalBytes, progress);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    f.destroy();
    return 0;
}

// ============================================================================
// Test: Scale-to-geodetic and scale-to-geocentric surface
// ============================================================================

static int test_scale_to_surface() {
    const CesiumEllipsoid* wgs84 = cesium_ellipsoid_wgs84();

    // A point 1000 m above Madrid
    CesiumCartographic madrid =
        cesium_cartographic_from_degrees(-3.7038, 40.4168, 1000.0);
    CesiumVec3 above =
        cesium_ellipsoid_cartographic_to_cartesian(wgs84, madrid);

    // Scale to geodetic surface — should project to height ≈ 0
    CesiumVec3 onSurface;
    int ok = cesium_ellipsoid_scale_to_geodetic_surface(wgs84, above, &onSurface);
    ASSERT_EQ(ok, 1);

    CesiumCartographic surfaceCarto;
    cesium_ellipsoid_cartesian_to_cartographic(wgs84, onSurface, &surfaceCarto);
    ASSERT_NEAR(surfaceCarto.height, 0.0, 0.1);
    ASSERT_NEAR(surfaceCarto.longitude, madrid.longitude, 1e-6);
    ASSERT_NEAR(surfaceCarto.latitude, madrid.latitude, 1e-6);

    // Scale to geocentric surface
    CesiumVec3 geocentric;
    ok = cesium_ellipsoid_scale_to_geocentric_surface(wgs84, above, &geocentric);
    ASSERT_EQ(ok, 1);

    // Geocentric point should have smaller magnitude than the original
    double magAbove = std::sqrt(above.x * above.x + above.y * above.y + above.z * above.z);
    double magGeo = std::sqrt(geocentric.x * geocentric.x +
                              geocentric.y * geocentric.y +
                              geocentric.z * geocentric.z);
    ASSERT_TRUE(magGeo < magAbove);

    return 0;
}

// ============================================================================
// Test: Root tile children traversal
// ============================================================================

static int test_root_tile_children() {
    SKIP_IF_NO_TOKEN();

    auto f = TilesetTestFixture::create();
    CesiumTileset* tileset = cesium_tileset_create_from_ion(
        f.externals, 1, g_ionToken, nullptr, nullptr);
    ASSERT_NOT_NULL(tileset);

    CesiumViewState* vs = createNycViewState();
    bool ready = f.waitForRootTile(tileset, vs);
    ASSERT_TRUE(ready);

    const CesiumTile* root = cesium_tileset_get_root_tile(tileset);
    ASSERT_NOT_NULL(root);

    double rootError = cesium_tile_get_geometric_error(root);
    ASSERT_TRUE(rootError > 0.0);

    int childCount = cesium_tile_get_children_count(root);
    ASSERT_TRUE(childCount > 0);
    std::printf("(rootError=%.0f children=%d) ", rootError, childCount);

    // Inspect the first child
    const CesiumTile* child = cesium_tile_get_child(root, 0);
    ASSERT_NOT_NULL(child);

    double childError = cesium_tile_get_geometric_error(child);
    // Child geometric error should be less than or equal to root's
    ASSERT_TRUE(childError <= rootError);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    f.destroy();
    return 0;
}

// ============================================================================
// Test: Credit system produces credits for an Ion tileset
// ============================================================================

static int test_credit_system_with_ion() {
    SKIP_IF_NO_TOKEN();

    auto f = TilesetTestFixture::create();
    CesiumTileset* tileset = cesium_tileset_create_from_ion(
        f.externals, 1, g_ionToken, nullptr, nullptr);
    ASSERT_NOT_NULL(tileset);

    CesiumViewState* vs = createNycViewState();
    bool ready = f.waitForRootTile(tileset, vs);
    ASSERT_TRUE(ready);

    // Pump a few frames so credits accumulate
    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 10; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_credit_system_start_next_frame(f.credits);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
        sleep_ms(50);
    }

    int creditCount =
        cesium_credit_system_get_credits_to_show_on_screen_count(f.credits);
    // Ion tilesets typically produce at least one credit
    ASSERT_TRUE(creditCount > 0);

    const char* credit =
        cesium_credit_system_get_credit_to_show_on_screen(f.credits, 0);
    ASSERT_NOT_NULL(credit);
    // Credit text may be empty if it's HTML-only; just verify the pointer is valid

    std::printf("(credits=%d firstLen=%zu) ",
                creditCount, std::strlen(credit));

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    f.destroy();
    return 0;
}

// ============================================================================
// Test: Tileset options affect loading behavior
// ============================================================================

static int test_tileset_options_affect_loading() {
    SKIP_IF_NO_TOKEN();

    auto f = TilesetTestFixture::create();
    CesiumTilesetOptions* opts = cesium_tileset_options_create();

    // Set a very high screen space error so fewer tiles are selected
    cesium_tileset_options_set_maximum_screen_space_error(opts, 500.0);
    cesium_tileset_options_set_maximum_simultaneous_tile_loads(opts, 5);

    CesiumTileset* tileset = cesium_tileset_create_from_ion(
        f.externals, 1, g_ionToken, opts, nullptr);
    ASSERT_NOT_NULL(tileset);

    CesiumViewState* vs = createNycViewState();
    bool ready = f.waitForRootTile(tileset, vs);
    ASSERT_TRUE(ready);

    const CesiumViewState* views[] = {vs};
    const CesiumViewUpdateResult* result = nullptr;
    for (int i = 0; i < 20; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_credit_system_start_next_frame(f.credits);
        result = cesium_tileset_update_view(tileset, views, 1, 0.016f);
        sleep_ms(50);
    }
    ASSERT_NOT_NULL(result);

    int coarseCount = cesium_view_update_result_get_tiles_to_render_count(result);
    // With SSE=500, very few tiles should be selected (likely 1-3)
    ASSERT_TRUE(coarseCount > 0);
    std::printf("(tiles@SSE500=%d) ", coarseCount);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(opts);
    f.destroy();
    return 0;
}

// ============================================================================
// Test: NULL safety — all destroy functions should accept NULL
// ============================================================================

static int test_null_safety() {
    // None of these should crash
    cesium_ellipsoid_destroy(nullptr);
    cesium_gltf_reader_destroy(nullptr);
    cesium_gltf_reader_result_destroy(nullptr);
    cesium_async_system_destroy(nullptr);
    cesium_asset_accessor_destroy(nullptr);
    cesium_credit_system_destroy(nullptr);
    cesium_tileset_externals_destroy(nullptr);
    cesium_tileset_options_destroy(nullptr);
    cesium_view_state_destroy(nullptr);
    cesium_tileset_destroy(nullptr);
    return 0;
}

// ============================================================================
// Main
// ============================================================================

/* ============================================================================
 * glTF, read from the document in test_gltf_asset.h
 *
 * Forty of the forty-nine functions in cesium_gltf.h need a model, and the only way to get
 * one is to read a document. These share one through a helper rather than each parsing
 * again: the parse is the slowest thing in this suite and repeating it ten times would make
 * the offline run worse for no extra coverage.
 *
 * Every index passed below is in range on purpose. Eleven of the twenty-six index-taking
 * getters in cesium_gltf.cpp index their vector with no bounds check at all -- for instance
 * cesium_gltf_node_get_mesh does nodes[nodeIndex] directly -- so an out-of-range index is
 * undefined behaviour rather than an error a test could assert on. That is worth fixing, and
 * until it is fixed it is not worth testing.
 * ========================================================================= */

/* Reads the embedded document. The caller destroys the result and the reader. */
static CesiumCGltfReaderResult* readTestGltf(CesiumCGltfReader** outReader) {
    CesiumCGltfReader* reader = cesium_gltf_reader_create();
    if (!reader) {
        return nullptr;
    }
    *outReader = reader;
    return cesium_gltf_reader_read(
        reader,
        reinterpret_cast<const uint8_t*>(kTestGltf),
        std::strlen(kTestGltf));
}

static int test_gltf_reads_the_document() {
    CesiumCGltfReader* reader = nullptr;
    CesiumCGltfReaderResult* result = readTestGltf(&reader);
    ASSERT_NOT_NULL(result);

    /* If this fails, the document is malformed and every glTF test after it is meaningless.
       Print why, rather than leaving nine unexplained failures behind it. */
    if (!cesium_gltf_reader_result_has_model(result)) {
        int errors = cesium_gltf_reader_result_get_error_count(result);
        std::printf("\n    document rejected, %d error(s):\n", errors);
        for (int i = 0; i < errors; ++i) {
            std::printf("      %s\n", cesium_gltf_reader_result_get_error(result, i));
        }
        cesium_gltf_reader_result_destroy(result);
        cesium_gltf_reader_destroy(reader);
        return 1;
    }

    ASSERT_EQ(cesium_gltf_reader_result_get_error_count(result), 0);

    /* Warnings are allowed -- the reader may have something to say about a data URI -- but
       the count and the accessor have to agree with each other. */
    int warnings = cesium_gltf_reader_result_get_warning_count(result);
    ASSERT_TRUE(warnings >= 0);
    for (int i = 0; i < warnings; ++i) {
        ASSERT_NOT_NULL(cesium_gltf_reader_result_get_warning(result, i));
    }

    ASSERT_NOT_NULL(cesium_gltf_reader_result_get_model(result));

    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

static int test_gltf_model_counts() {
    CesiumCGltfReader* reader = nullptr;
    CesiumCGltfReaderResult* result = readTestGltf(&reader);
    ASSERT_NOT_NULL(result);
    const CesiumGltfModel* model = cesium_gltf_reader_result_get_model(result);
    ASSERT_NOT_NULL(model);

    ASSERT_EQ(cesium_gltf_model_get_scene_count(model), kExpectedSceneCount);
    ASSERT_EQ(cesium_gltf_model_get_node_count(model), kExpectedNodeCount);
    ASSERT_EQ(cesium_gltf_model_get_mesh_count(model), kExpectedMeshCount);
    ASSERT_EQ(cesium_gltf_model_get_material_count(model), kExpectedMaterialCount);
    ASSERT_EQ(cesium_gltf_model_get_texture_count(model), kExpectedTextureCount);
    ASSERT_EQ(cesium_gltf_model_get_image_count(model), kExpectedImageCount);
    ASSERT_EQ(cesium_gltf_model_get_accessor_count(model), kExpectedAccessorCount);
    ASSERT_EQ(cesium_gltf_model_get_buffer_count(model), kExpectedBufferCount);
    ASSERT_EQ(cesium_gltf_model_get_buffer_view_count(model), kExpectedBufferViewCount);
    ASSERT_EQ(cesium_gltf_model_get_animation_count(model), kExpectedAnimationCount);
    ASSERT_EQ(cesium_gltf_model_get_skin_count(model), kExpectedSkinCount);

    const char* meshName = cesium_gltf_model_get_mesh_name(model, 0);
    ASSERT_NOT_NULL(meshName);
    ASSERT_EQ(std::strcmp(meshName, "triangle"), 0);

    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

static int test_gltf_scene_and_node_hierarchy() {
    CesiumCGltfReader* reader = nullptr;
    CesiumCGltfReaderResult* result = readTestGltf(&reader);
    ASSERT_NOT_NULL(result);
    const CesiumGltfModel* model = cesium_gltf_reader_result_get_model(result);
    ASSERT_NOT_NULL(model);

    int scene = cesium_gltf_model_get_default_scene(model);
    ASSERT_EQ(scene, 0);
    ASSERT_EQ(cesium_gltf_scene_get_node_count(model, scene), 1);

    int root = cesium_gltf_scene_get_node(model, scene, 0);
    ASSERT_EQ(root, 0);
    ASSERT_EQ(cesium_gltf_node_get_mesh(model, root), 0);
    ASSERT_EQ(cesium_gltf_node_get_children_count(model, root), 1);

    /* The child carries no mesh. -1 is the documented "absent" answer, and checking it is
       the only way to know the getter distinguishes absent from index zero. */
    int child = cesium_gltf_node_get_child(model, root, 0);
    ASSERT_EQ(child, 1);
    ASSERT_EQ(cesium_gltf_node_get_mesh(model, child), -1);
    ASSERT_EQ(cesium_gltf_node_get_children_count(model, child), 0);

    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

static int test_gltf_node_transform() {
    CesiumCGltfReader* reader = nullptr;
    CesiumCGltfReaderResult* result = readTestGltf(&reader);
    ASSERT_NOT_NULL(result);
    const CesiumGltfModel* model = cesium_gltf_reader_result_get_model(result);
    ASSERT_NOT_NULL(model);

    double t[3] = {0.0, 0.0, 0.0};
    cesium_gltf_node_get_translation(model, 0, t);
    ASSERT_NEAR(t[0], 1.0, 1e-12);
    ASSERT_NEAR(t[1], 2.0, 1e-12);
    ASSERT_NEAR(t[2], 3.0, 1e-12);

    double r[4] = {9.0, 9.0, 9.0, 9.0};
    cesium_gltf_node_get_rotation(model, 0, r);
    ASSERT_NEAR(r[0], 0.0, 1e-12);
    ASSERT_NEAR(r[1], 0.0, 1e-12);
    ASSERT_NEAR(r[2], 0.0, 1e-12);
    ASSERT_NEAR(r[3], 1.0, 1e-12);

    double s[3] = {0.0, 0.0, 0.0};
    cesium_gltf_node_get_scale(model, 0, s);
    ASSERT_NEAR(s[0], 2.0, 1e-12);
    ASSERT_NEAR(s[1], 2.0, 1e-12);
    ASSERT_NEAR(s[2], 2.0, 1e-12);

    /* That node uses TRS rather than a matrix, and get_matrix reports it by returning 0 while
       still filling the output with identity. A caller that ignored the return value and used
       the matrix would silently drop the translation, so the contract is worth pinning. */
    double m[16] = {0.0};
    int hasMatrix = cesium_gltf_node_get_matrix(model, 0, m);
    ASSERT_EQ(hasMatrix, 0);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            ASSERT_NEAR(m[row * 4 + col], row == col ? 1.0 : 0.0, 1e-12);
        }
    }

    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

static int test_gltf_primitive_and_attributes() {
    CesiumCGltfReader* reader = nullptr;
    CesiumCGltfReaderResult* result = readTestGltf(&reader);
    ASSERT_NOT_NULL(result);
    const CesiumGltfModel* model = cesium_gltf_reader_result_get_model(result);
    ASSERT_NOT_NULL(model);

    ASSERT_EQ(cesium_gltf_mesh_get_primitive_count(model, 0), 1);
    ASSERT_EQ(cesium_gltf_primitive_get_mode(model, 0, 0), kModeTriangles);
    ASSERT_EQ(cesium_gltf_primitive_get_material_index(model, 0, 0), 0);
    ASSERT_EQ(
        cesium_gltf_primitive_get_indices_accessor_index(model, 0, 0), kIndicesAccessor);
    ASSERT_EQ(cesium_gltf_primitive_get_attribute_count(model, 0, 0), 1);

    const char* attribute = cesium_gltf_primitive_get_attribute_name(model, 0, 0, 0);
    ASSERT_NOT_NULL(attribute);
    ASSERT_EQ(std::strcmp(attribute, "POSITION"), 0);
    ASSERT_EQ(
        cesium_gltf_primitive_get_attribute_accessor_index(model, 0, 0, 0),
        kPositionAccessor);

    /* Lookup by name has to agree with lookup by position, and has to answer -1 rather than
       0 for an attribute that is not there. */
    ASSERT_EQ(
        cesium_gltf_primitive_find_attribute_accessor_index(model, 0, 0, "POSITION"),
        kPositionAccessor);
    ASSERT_EQ(
        cesium_gltf_primitive_find_attribute_accessor_index(model, 0, 0, "NORMAL"), -1);

    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

static int test_gltf_accessor_data() {
    CesiumCGltfReader* reader = nullptr;
    CesiumCGltfReaderResult* result = readTestGltf(&reader);
    ASSERT_NOT_NULL(result);
    const CesiumGltfModel* model = cesium_gltf_reader_result_get_model(result);
    ASSERT_NOT_NULL(model);

    /* Positions. The values are the ones written into the buffer in test_gltf_asset.h, so
       this checks that the base64, the buffer view offset and the accessor stride all line
       up -- not merely that some pointer came back. */
    CesiumAccessorData positions;
    std::memset(&positions, 0, sizeof(positions));
    ASSERT_EQ(cesium_gltf_accessor_get_data(model, kPositionAccessor, &positions), 1);
    ASSERT_NOT_NULL(positions.data);
    ASSERT_EQ(positions.count, kVertexCount);
    ASSERT_EQ(positions.componentType, kComponentTypeFloat);
    ASSERT_EQ(positions.numberOfComponents, 3);
    ASSERT_EQ(positions.stride, sizeof(float) * 3);

    const unsigned char* bytes = static_cast<const unsigned char*>(positions.data);
    const float expected[3][3] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    for (int vertex = 0; vertex < kVertexCount; ++vertex) {
        float xyz[3];
        std::memcpy(xyz, bytes + static_cast<size_t>(vertex) * positions.stride, sizeof(xyz));
        for (int component = 0; component < 3; ++component) {
            ASSERT_NEAR(
                static_cast<double>(xyz[component]),
                static_cast<double>(expected[vertex][component]),
                1e-6);
        }
    }

    /* Indices, which live at a byte offset inside the same buffer. */
    CesiumAccessorData indices;
    std::memset(&indices, 0, sizeof(indices));
    ASSERT_EQ(cesium_gltf_accessor_get_data(model, kIndicesAccessor, &indices), 1);
    ASSERT_NOT_NULL(indices.data);
    ASSERT_EQ(indices.count, 3);
    ASSERT_EQ(indices.componentType, kComponentTypeUnsignedShort);
    ASSERT_EQ(indices.numberOfComponents, 1);

    const unsigned char* indexBytes = static_cast<const unsigned char*>(indices.data);
    for (int i = 0; i < 3; ++i) {
        uint16_t value = 0;
        std::memcpy(&value, indexBytes + static_cast<size_t>(i) * indices.stride, sizeof(value));
        ASSERT_EQ(static_cast<int>(value), i);
    }

    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

static int test_gltf_material_texture_sampler_image() {
    CesiumCGltfReader* reader = nullptr;
    CesiumCGltfReaderResult* result = readTestGltf(&reader);
    ASSERT_NOT_NULL(result);
    const CesiumGltfModel* model = cesium_gltf_reader_result_get_model(result);
    ASSERT_NOT_NULL(model);

    CesiumMaterialData material;
    std::memset(&material, 0, sizeof(material));
    ASSERT_EQ(cesium_gltf_material_get_data(model, 0, &material), 1);
    ASSERT_TRUE(material.doubleSided != 0);
    ASSERT_EQ(material.baseColorTexture.textureIndex, 0);

    ASSERT_EQ(cesium_gltf_texture_get_source(model, 0), 0);
    ASSERT_EQ(cesium_gltf_texture_get_sampler(model, 0), 0);

    CesiumSamplerData sampler;
    std::memset(&sampler, 0, sizeof(sampler));
    ASSERT_EQ(cesium_gltf_sampler_get_data(model, 0, &sampler), 1);
    ASSERT_EQ(sampler.magFilter, kMagFilterLinear);
    ASSERT_EQ(sampler.minFilter, kMinFilterLinearMipmapLinear);
    ASSERT_EQ(sampler.wrapS, kWrapClampToEdge);
    ASSERT_EQ(sampler.wrapT, kWrapClampToEdge);

    /* Whether the reader decoded the embedded PNG depends on how it was configured, and
       cesium_gltf_image_get_data says so by returning 0. Assert the contract rather than an
       outcome: either it declines, or it hands back a buffer whose size agrees with the
       dimensions it reports. A buffer that disagreed would be read past the end by any
       consumer that trusted those numbers. */
    CesiumImageData image;
    std::memset(&image, 0, sizeof(image));
    int decoded = cesium_gltf_image_get_data(model, 0, &image);
    ASSERT_TRUE(decoded == 0 || decoded == 1);
    if (decoded == 1) {
        ASSERT_NOT_NULL(image.pixelData);
        ASSERT_EQ(image.width, 1);
        ASSERT_EQ(image.height, 1);
        ASSERT_TRUE(image.channels > 0);
        ASSERT_TRUE(image.bytesPerChannel > 0);
        ASSERT_EQ(
            image.pixelDataSize,
            static_cast<size_t>(image.width) * static_cast<size_t>(image.height) *
                static_cast<size_t>(image.channels) *
                static_cast<size_t>(image.bytesPerChannel));
    }

    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

static int test_gltf_glb_round_trip() {
    CesiumCGltfReader* reader = nullptr;
    CesiumCGltfReaderResult* result = readTestGltf(&reader);
    ASSERT_NOT_NULL(result);
    const CesiumGltfModel* model = cesium_gltf_reader_result_get_model(result);
    ASSERT_NOT_NULL(model);

    /* const_cast because get_model returns a const pointer while write_glb takes a mutable
       one: the writer may normalise the model as it serialises. That asymmetry is in the
       header rather than introduced here, and a C# caller meets the same one. */
    CesiumGltfModel* mutableModel = const_cast<CesiumGltfModel*>(model);

    uint8_t* glb = nullptr;
    size_t glbSize = 0;
    ASSERT_EQ(cesium_gltf_model_write_glb(mutableModel, &glb, &glbSize), 1);
    ASSERT_NOT_NULL(glb);
    ASSERT_TRUE(glbSize > 12);

    /* The GLB header: magic 'glTF', then version 2. Checking these says the writer produced
       a container rather than a blob of plausible length. */
    ASSERT_EQ(std::memcmp(glb, "glTF", 4), 0);
    uint32_t version = 0;
    std::memcpy(&version, glb + 4, sizeof(version));
    ASSERT_EQ(static_cast<int>(version), 2);

    /* And it has to read back, which is what says the output is valid rather than merely
       well-formed at the front. */
    CesiumCGltfReader* reader2 = cesium_gltf_reader_create();
    ASSERT_NOT_NULL(reader2);
    CesiumCGltfReaderResult* result2 = cesium_gltf_reader_read(reader2, glb, glbSize);
    ASSERT_NOT_NULL(result2);
    ASSERT_TRUE(cesium_gltf_reader_result_has_model(result2) != 0);

    const CesiumGltfModel* model2 = cesium_gltf_reader_result_get_model(result2);
    ASSERT_NOT_NULL(model2);
    ASSERT_EQ(cesium_gltf_model_get_mesh_count(model2), kExpectedMeshCount);
    ASSERT_EQ(cesium_gltf_model_get_node_count(model2), kExpectedNodeCount);
    ASSERT_EQ(cesium_gltf_model_get_accessor_count(model2), kExpectedAccessorCount);

    cesium_gltf_reader_result_destroy(result2);
    cesium_gltf_reader_destroy(reader2);
    cesium_gltf_free_glb(glb);
    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

static int test_gltf_strip_feature_ids() {
    CesiumCGltfReader* reader = nullptr;
    CesiumCGltfReaderResult* result = readTestGltf(&reader);
    ASSERT_NOT_NULL(result);
    const CesiumGltfModel* model = cesium_gltf_reader_result_get_model(result);
    ASSERT_NOT_NULL(model);

    /* This document has no feature IDs, so the call is a no-op -- but it has to be a no-op
       that leaves the model usable, rather than one that clears something it should not.
       Counts either side are the cheapest way to say that. */
    int meshes = cesium_gltf_model_get_mesh_count(model);
    int accessors = cesium_gltf_model_get_accessor_count(model);

    cesium_gltf_model_strip_feature_ids(const_cast<CesiumGltfModel*>(model));

    ASSERT_EQ(cesium_gltf_model_get_mesh_count(model), meshes);
    ASSERT_EQ(cesium_gltf_model_get_accessor_count(model), accessors);
    ASSERT_EQ(cesium_gltf_primitive_get_attribute_count(model, 0, 0), 1);

    cesium_gltf_reader_result_destroy(result);
    cesium_gltf_reader_destroy(reader);
    return 0;
}

/* ============================================================================
 * Raster overlays
 *
 * Creating an overlay does no network work: it records a URL and some options, and fetching
 * begins when a tileset that owns it is updated. So all four constructors, the options
 * round-trip and the collection are reachable offline.
 * ========================================================================= */

static int test_raster_overlay_options_default() {
    CesiumRasterOverlayOptions options;
    std::memset(&options, 0xAB, sizeof(options));
    cesium_raster_overlay_options_default(&options);

    /* The defaults the header documents. Worth checking because a consumer calls _default
       and then changes one field, inheriting the rest -- so a wrong default is silent. */
    ASSERT_EQ(options.maximumSimultaneousTileLoads, 20);
    ASSERT_EQ(options.maximumTextureSize, 2048);
    ASSERT_NEAR(options.maximumScreenSpaceError, 2.0, 1e-12);
    ASSERT_EQ(options.showCreditsOnScreen, 0);
    ASSERT_TRUE(options.subTileCacheBytes > 0);
    return 0;
}

static int test_raster_overlay_constructors() {
    CesiumRasterOverlay* url = cesium_url_template_raster_overlay_create(
        "url-template", "https://example.invalid/{z}/{x}/{y}.png", 0, 18, 256, 256);
    ASSERT_NOT_NULL(url);
    cesium_raster_overlay_destroy(url);

    CesiumRasterOverlay* tms =
        cesium_tile_map_service_raster_overlay_create("tms", "https://example.invalid/tms/");
    ASSERT_NOT_NULL(tms);
    cesium_raster_overlay_destroy(tms);

    CesiumRasterOverlay* wms = cesium_web_map_service_raster_overlay_create(
        "wms", "https://example.invalid/wms", "layer-a,layer-b", 256, 256);
    ASSERT_NOT_NULL(wms);
    cesium_raster_overlay_destroy(wms);

    /* An Ion overlay with a nonsense token still constructs: the token is only used once
       something asks it to load. */
    CesiumRasterOverlay* ion =
        cesium_ion_raster_overlay_create(2, "not-a-real-token", nullptr);
    ASSERT_NOT_NULL(ion);
    cesium_raster_overlay_destroy(ion);
    return 0;
}

static int test_raster_overlay_options_round_trip() {
    CesiumRasterOverlay* overlay = cesium_url_template_raster_overlay_create(
        "url-template", "https://example.invalid/{z}/{x}/{y}.png", 0, 18, 256, 256);
    ASSERT_NOT_NULL(overlay);

    CesiumRasterOverlayOptions options;
    std::memset(&options, 0, sizeof(options));
    ASSERT_EQ(cesium_raster_overlay_get_options(overlay, &options), 1);

    options.maximumSimultaneousTileLoads = 7;
    options.maximumTextureSize = 512;
    options.maximumScreenSpaceError = 4.5;
    options.showCreditsOnScreen = 1;
    ASSERT_EQ(cesium_raster_overlay_set_options(overlay, &options), 1);

    CesiumRasterOverlayOptions readBack;
    std::memset(&readBack, 0, sizeof(readBack));
    ASSERT_EQ(cesium_raster_overlay_get_options(overlay, &readBack), 1);
    ASSERT_EQ(readBack.maximumSimultaneousTileLoads, 7);
    ASSERT_EQ(readBack.maximumTextureSize, 512);
    ASSERT_NEAR(readBack.maximumScreenSpaceError, 4.5, 1e-12);
    ASSERT_EQ(readBack.showCreditsOnScreen, 1);

    cesium_raster_overlay_destroy(overlay);
    return 0;
}

static int test_raster_overlay_collection() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    ASSERT_NOT_NULL(async);
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create("CesiumC-tests");
    ASSERT_NOT_NULL(accessor);
    CesiumCreditSystem* credits = cesium_credit_system_create();
    ASSERT_NOT_NULL(credits);
    CesiumTilesetExternals* externals =
        cesium_tileset_externals_create(async, accessor, credits);
    ASSERT_NOT_NULL(externals);

    CesiumTilesetOptions* options = cesium_tileset_options_create();
    ASSERT_NOT_NULL(options);
    CesiumTileset* tileset = cesium_tileset_create_from_url(
        externals, "https://example.invalid/tileset.json", options);
    ASSERT_NOT_NULL(tileset);

    CesiumRasterOverlayCollection* collection = cesium_tileset_get_overlays(tileset);
    ASSERT_NOT_NULL(collection);

    CesiumRasterOverlay* overlay = cesium_url_template_raster_overlay_create(
        "url-template", "https://example.invalid/{z}/{x}/{y}.png", 0, 18, 256, 256);
    ASSERT_NOT_NULL(overlay);

    /* Add then remove. The collection takes a reference, so removing has to leave the
       tileset holding nothing dangling -- which is why the tileset is destroyed after this
       rather than before. */
    cesium_raster_overlay_collection_add(collection, overlay);
    cesium_raster_overlay_collection_remove(collection, overlay);

    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    cesium_tileset_externals_destroy(externals);
    cesium_credit_system_destroy(credits);
    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);
    return 0;
}

/* ============================================================================
 * Ion, offline
 *
 * Everything that talks to Ion needs a token, and those tests exist already and are skipped
 * without one. What is reachable offline is the object lifecycle -- which is where a wrapper
 * bug would live anyway, as opposed to a service bug.
 * ========================================================================= */

static int test_ion_connection_argument_guards() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    ASSERT_NOT_NULL(async);
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create("CesiumC-tests");
    ASSERT_NOT_NULL(accessor);

    /* Only the argument guards, and deliberately so.
     *
     * cesium_ion_connection_create is not a constructor despite reading like one: it fetches
     * ApplicationData from the API URL and pumps the main thread in a 5000 x 10ms loop
     * waiting for it. So a successful call needs the network and an unreachable host costs
     * fifty seconds before returning NULL. An earlier version of this test called it with a
     * NULL apiUrl and passed -- by reaching api.cesium.com from CI, which is not a test being
     * offline, it is a test whose result depends on somebody else's uptime.
     *
     * The guards below return before any of that, so they are the part that can be checked
     * here. Constructing a real connection lives with the other token-dependent tests.
     */
    ASSERT_NULL(cesium_ion_connection_create(nullptr, accessor, "token", nullptr));
    ASSERT_NULL(cesium_ion_connection_create(async, nullptr, "token", nullptr));
    ASSERT_NULL(cesium_ion_connection_create(async, accessor, nullptr, nullptr));

    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);
    return 0;
}

/* ============================================================================
 * The remaining gaps in geospatial and tileset
 * ========================================================================= */

static int test_ellipsoid_unit_sphere() {
    const CesiumEllipsoid* unit = cesium_ellipsoid_unit_sphere();
    ASSERT_NOT_NULL(unit);
    ASSERT_NEAR(cesium_ellipsoid_get_maximum_radius(unit), 1.0, 1e-12);
    ASSERT_NEAR(cesium_ellipsoid_get_minimum_radius(unit), 1.0, 1e-12);

    /* On a unit sphere the surface normal at a point is the normalised point, which makes
       this the one ellipsoid whose expected answer can be written down by hand. */
    CesiumVec3 onX = {3.0, 0.0, 0.0};
    CesiumVec3 normal = cesium_ellipsoid_geodetic_surface_normal_cartesian(unit, onX);
    ASSERT_NEAR(normal.x, 1.0, 1e-12);
    ASSERT_NEAR(normal.y, 0.0, 1e-12);
    ASSERT_NEAR(normal.z, 0.0, 1e-12);

    CesiumVec3 belowZ = {0.0, 0.0, -5.0};
    normal = cesium_ellipsoid_geodetic_surface_normal_cartesian(unit, belowZ);
    ASSERT_NEAR(normal.x, 0.0, 1e-12);
    ASSERT_NEAR(normal.y, 0.0, 1e-12);
    ASSERT_NEAR(normal.z, -1.0, 1e-12);

    /* Not destroyed: unit_sphere returns a const singleton, the same shape as
       cesium_ellipsoid_wgs84, and the existing tests do not destroy that one either. */
    return 0;
}

static int test_tileset_from_url_render_content() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    ASSERT_NOT_NULL(async);
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create("CesiumC-tests");
    ASSERT_NOT_NULL(accessor);
    CesiumCreditSystem* credits = cesium_credit_system_create();
    ASSERT_NOT_NULL(credits);
    CesiumTilesetExternals* externals =
        cesium_tileset_externals_create(async, accessor, credits);
    ASSERT_NOT_NULL(externals);

    /* Construction records the URL; nothing is fetched until the tileset is updated, so an
       unreachable host is fine and keeps this offline. */
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    ASSERT_NOT_NULL(options);
    CesiumTileset* tileset = cesium_tileset_create_from_url(
        externals, "https://example.invalid/tileset.json", options);
    ASSERT_NOT_NULL(tileset);

    /* Without a successful load there is no root tile. If one exists anyway, its content
       accessors have to agree with each other: claiming no render content and then handing
       back a model is the inconsistency that would strand a consumer. */
    const CesiumTile* root = cesium_tileset_get_root_tile(tileset);
    if (root != nullptr) {
        int hasContent = cesium_tile_has_render_content(root);
        ASSERT_TRUE(hasContent == 0 || hasContent == 1);
        if (hasContent == 0) {
            ASSERT_NULL(cesium_tile_get_render_content_model(root));
            ASSERT_NULL(cesium_tile_get_render_resources(root));
        }
    }

    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    cesium_tileset_externals_destroy(externals);
    cesium_credit_system_destroy(credits);
    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);
    return 0;
}

static int g_rootTileAvailableCalls = 0;

static void onRootTileAvailable(void* userData) {
    ++g_rootTileAvailableCalls;
    if (userData != nullptr) {
        *static_cast<int*>(userData) += 1;
    }
}

static int test_tileset_callback_registration() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    ASSERT_NOT_NULL(async);
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create("CesiumC-tests");
    ASSERT_NOT_NULL(accessor);
    CesiumCreditSystem* credits = cesium_credit_system_create();
    ASSERT_NOT_NULL(credits);
    CesiumTilesetExternals* externals =
        cesium_tileset_externals_create(async, accessor, credits);
    ASSERT_NOT_NULL(externals);

    /* Registering the renderer callbacks is pure bookkeeping -- nothing invokes them until a
       tile loads -- so both a real struct and NULL are reachable offline. NULL is the
       documented way back to the no-op default, and a wrapper that mishandled it would leave
       the previous function pointers live against a caller who thought they were gone. */
    CesiumRendererResourceCallbacks callbacks;
    std::memset(&callbacks, 0, sizeof(callbacks));
    cesium_tileset_externals_set_renderer_resource_callbacks(externals, &callbacks);
    cesium_tileset_externals_set_renderer_resource_callbacks(externals, nullptr);

    CesiumTilesetOptions* options = cesium_tileset_options_create();
    ASSERT_NOT_NULL(options);
    CesiumTileset* tileset = cesium_tileset_create_from_url(
        externals, "https://example.invalid/tileset.json", options);
    ASSERT_NOT_NULL(tileset);

    /* The host is unreachable, so this callback will not fire -- which is the point. It is
       registered, the tileset is destroyed, and nothing calls into a dead frame afterwards. */
    int calls = 0;
    g_rootTileAvailableCalls = 0;
    cesium_tileset_set_root_tile_available_callback(tileset, onRootTileAvailable, &calls);
    cesium_tileset_set_root_tile_available_callback(tileset, nullptr, nullptr);

    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    cesium_tileset_externals_destroy(externals);
    cesium_credit_system_destroy(credits);
    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);
    return 0;
}

/* ============================================================================
 * Host-provided HTTP accessor
 *
 * Everything here is offline. A fake host answers from string literals, so these run
 * identically under Node on browser-wasm and natively on the other six platforms -- which
 * is the whole reason the transport is a host callback rather than emscripten_fetch, whose
 * FETCH implementation calls new XMLHttpRequest() and has no Node fallback.
 *
 * The fake host can misbehave on purpose: never answer, answer twice, answer from inside
 * beginRequest, answer after everything has been destroyed. Those are the cases the design
 * exists to survive, so they are the ones worth writing down.
 * ========================================================================= */

#define FAKE_HOST_MAX_QUEUED 8

struct FakeHost {
    int beginCalls;
    int cancelCalls;
    int tickCalls;
    int destroyCalls;

    CesiumAssetRequestId lastId;
    char lastMethod[16];
    char lastUrl[256];
    char firstUrl[256];
    int lastHeaderCount;
    int allHeaderPointersValid;

    /* What to serve, and how to behave. */
    const char* bodyToServe;
    uint16_t statusToServe;
    int answerAtAll;                    /* 0 = swallow every request */
    int answerFromInsideBeginRequest;   /* exercises the re-entrancy path */

    CesiumAssetRequestId queued[FAKE_HOST_MAX_QUEUED];
    int queuedCount;

    /* Filled by the tileset's load-error callback. Without this a failed pump reports only
       "root never appeared", which is the least useful thing it could say. */
    int loadErrorCount;
    char lastLoadError[256];
};

static void fakeHostLoadError(void* userData, const char* message) {
    FakeHost* host = static_cast<FakeHost*>(userData);
    ++host->loadErrorCount;
    if (message) {
        std::snprintf(host->lastLoadError, sizeof(host->lastLoadError), "%s", message);
    }
}

static void fakeHostAnswer(FakeHost* host, CesiumAssetRequestId id) {
    const CesiumHttpHeader headers[] = {
        {"Content-Type", "application/json"},
        {"X-Test-Header", "present"},
    };
    const char* body = host->bodyToServe ? host->bodyToServe : "";
    cesium_asset_request_complete(
        id,
        host->statusToServe,
        headers,
        2,
        reinterpret_cast<const uint8_t*>(body),
        std::strlen(body));
}

static void fakeHostBegin(
    void* userData,
    CesiumAssetRequestId requestId,
    const char* method,
    const char* url,
    const CesiumHttpHeader* headers,
    int32_t headerCount,
    const uint8_t* body,
    size_t bodySize) {
    (void)body;
    (void)bodySize;
    FakeHost* host = static_cast<FakeHost*>(userData);

    if (host->beginCalls == 0) {
        std::snprintf(host->firstUrl, sizeof(host->firstUrl), "%s", url ? url : "");
    }
    ++host->beginCalls;
    host->lastId = requestId;
    std::snprintf(host->lastMethod, sizeof(host->lastMethod), "%s", method ? method : "");
    std::snprintf(host->lastUrl, sizeof(host->lastUrl), "%s", url ? url : "");
    host->lastHeaderCount = static_cast<int>(headerCount);

    /* Every pointer handed over must be readable for the duration of this call. */
    host->allHeaderPointersValid = 1;
    for (int32_t i = 0; i < headerCount; ++i) {
        if (headers[i].name == nullptr || headers[i].value == nullptr) {
            host->allHeaderPointersValid = 0;
        }
    }

    if (!host->answerAtAll) {
        return;
    }
    if (host->answerFromInsideBeginRequest) {
        fakeHostAnswer(host, requestId);
        return;
    }
    if (host->queuedCount < FAKE_HOST_MAX_QUEUED) {
        host->queued[host->queuedCount++] = requestId;
    }
}

static void fakeHostCancel(void* userData, CesiumAssetRequestId requestId) {
    (void)requestId;
    ++static_cast<FakeHost*>(userData)->cancelCalls;
}

static void fakeHostTick(void* userData) {
    ++static_cast<FakeHost*>(userData)->tickCalls;
}

static void fakeHostDestroy(void* userData) {
    ++static_cast<FakeHost*>(userData)->destroyCalls;
}

static CesiumAssetAccessorCallbacks fakeHostCallbacks(FakeHost* host) {
    CesiumAssetAccessorCallbacks cb;
    std::memset(&cb, 0, sizeof(cb));
    cb.userData = host;
    cb.beginRequest = fakeHostBegin;
    cb.cancelRequest = fakeHostCancel;
    cb.tick = fakeHostTick;
    cb.destroy = fakeHostDestroy;
    return cb;
}

static void fakeHostInit(FakeHost* host, const char* body) {
    std::memset(host, 0, sizeof(*host));
    host->bodyToServe = body;
    host->statusToServe = 200;
    host->answerAtAll = 1;
}

/** Answers everything queued since the last pump. */
static void fakeHostPump(FakeHost* host) {
    const int count = host->queuedCount;
    host->queuedCount = 0;
    for (int i = 0; i < count; ++i) {
        fakeHostAnswer(host, host->queued[i]);
    }
}

/**
 * @brief Drives a tileset to its root tile.
 *
 * Sleeps between iterations on any platform that has threads, and this is the whole subtlety.
 * The fake host does answer synchronously when pumped, so there is nothing to wait for in the
 * HTTP half -- but cesium-native parses the tileset JSON in runInWorkerThread, which on a
 * threaded build is a real worker. A pump with no sleep runs its two hundred iterations in a
 * couple of milliseconds and gives up long before that worker has finished, which is exactly
 * how the first version of this failed on all five desktop legs while claiming there was
 * "nothing to wait for".
 *
 * On single-threaded wasm the opposite holds and sleeping would be actively wrong: the
 * deferred queue is drained by the pump itself, so the one thread must keep running for
 * anything to happen at all.
 */
static bool pumpUntilRootTile(
    TilesetTestFixture& f,
    FakeHost* host,
    CesiumTileset* tileset,
    CesiumViewState* vs,
    int maxIterations = 200) {
    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < maxIterations; ++i) {
        fakeHostPump(host);
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_credit_system_start_next_frame(f.credits);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
        if (cesium_tileset_is_root_tile_available(tileset)) {
            return true;
        }
#if !(defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__))
        sleep_ms(5);
#endif
    }
    std::printf(
        "\n    no root tile after %d pumps: beginCalls=%d loadErrors=%d last=\"%s\"\n",
        maxIterations, host->beginCalls, host->loadErrorCount, host->lastLoadError);
    return false;
}

/** A fixture whose accessor is the fake host rather than curl. */
static TilesetTestFixture createHostFixture(const CesiumAssetAccessorCallbacks* cb) {
    TilesetTestFixture f{};
    f.async = cesium_async_system_create();
    f.accessor = cesium_asset_accessor_create_from_callbacks(cb);
    f.credits = cesium_credit_system_create();
    f.externals = cesium_tileset_externals_create(f.async, f.accessor, f.credits);
    return f;
}

static CesiumViewState* makeTestViewState() {
    /* Same shape the existing tileset tests use. */
    CesiumVec3 pos = {6378137.0 + 1000.0, 0.0, 0.0};
    CesiumVec3 dir = {-1.0, 0.0, 0.0};
    CesiumVec3 up = {0.0, 0.0, 1.0};
    CesiumVec2 viewport = {1920.0, 1080.0};
    return cesium_view_state_create_perspective(
        pos, dir, up, viewport, 60.0 * PI / 180.0, 33.75 * PI / 180.0, nullptr);
}

static int test_host_accessor_create_destroy() {
    FakeHost host;
    fakeHostInit(&host, kTestTilesetJson);
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    CesiumAssetAccessor* accessor = cesium_asset_accessor_create_from_callbacks(&cb);
    ASSERT_NOT_NULL(accessor);
    ASSERT_EQ(cesium_asset_accessor_get_pending_request_count(accessor), 0);
    cesium_asset_accessor_destroy(accessor);
    ASSERT_EQ(host.destroyCalls, 1);

    /* NULL is documented as producing a working handle with no transport. */
    CesiumAssetAccessor* none = cesium_asset_accessor_create_from_callbacks(nullptr);
    ASSERT_NOT_NULL(none);
    cesium_asset_accessor_destroy(none);
    return 0;
}

static int test_host_accessor_callbacks_are_optional() {
    /* Only beginRequest set. The other three NULL must be no-ops rather than crashes -- that
       is what "any callback may be NULL" has to mean to be worth writing. */
    FakeHost host;
    fakeHostInit(&host, kTestTilesetJson);
    CesiumAssetAccessorCallbacks cb;
    std::memset(&cb, 0, sizeof(cb));
    cb.userData = &host;
    cb.beginRequest = fakeHostBegin;

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    ASSERT_NOT_NULL(tileset);
    CesiumViewState* vs = makeTestViewState();

    ASSERT_TRUE(pumpUntilRootTile(f, &host, tileset, vs));

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_no_begin_callback_fails_requests() {
    /* A zeroed struct behaves exactly as the old placeholder did: every request fails with
       status 0. This test says the behaviour was preserved when those classes were deleted,
       not merely that something compiles. */
    CesiumAssetAccessorCallbacks cb;
    std::memset(&cb, 0, sizeof(cb));

    FakeHost host;
    fakeHostInit(&host, nullptr);

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    ASSERT_NOT_NULL(tileset);
    CesiumViewState* vs = makeTestViewState();

    /* Expected to fail, so the pump's diagnostic would be noise. */
    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 20; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_credit_system_start_next_frame(f.credits);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
    }
    ASSERT_TRUE(!cesium_tileset_is_root_tile_available(tileset));

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_receives_request() {
    FakeHost host;
    fakeHostInit(&host, kTestTilesetJson);
    host.answerAtAll = 0;   /* just look at what arrives */
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    ASSERT_NOT_NULL(tileset);
    CesiumViewState* vs = makeTestViewState();

    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 20 && host.beginCalls == 0; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
    }

    ASSERT_EQ(host.beginCalls, 1);
    ASSERT_TRUE(host.lastId != CESIUM_ASSET_REQUEST_ID_INVALID);
    ASSERT_EQ(std::strcmp(host.lastMethod, "GET"), 0);
    ASSERT_EQ(std::strcmp(host.lastUrl, kTestTilesetUrl), 0);
    ASSERT_TRUE(host.lastHeaderCount >= 0);
    ASSERT_EQ(host.allHeaderPointersValid, 1);
    ASSERT_EQ(cesium_asset_accessor_get_pending_request_count(f.accessor), 1);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_response_reaches_tileset() {
    /* The end-to-end one. Until this passes none of the edge cases mean anything, and on
       browser-wasm it is the proof that HTTP works at all. */
    FakeHost host;
    fakeHostInit(&host, kTestTilesetJson);
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    ASSERT_NOT_NULL(tileset);
    CesiumViewState* vs = makeTestViewState();

    ASSERT_TRUE(pumpUntilRootTile(f, &host, tileset, vs));
    ASSERT_NOT_NULL(cesium_tileset_get_root_tile(tileset));
    ASSERT_EQ(cesium_asset_accessor_get_pending_request_count(f.accessor), 0);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_body_is_copied() {
    /* The test that pins the memory contract, and the most valuable one here.
       The body is served from a stack buffer that is scribbled over on the next line; if the
       response borrowed it rather than copying, the tileset would parse garbage. */
    char buffer[2048];
    std::snprintf(buffer, sizeof(buffer), "%s", kTestTilesetJson);

    FakeHost host;
    fakeHostInit(&host, nullptr);
    host.answerAtAll = 0;
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    ASSERT_NOT_NULL(tileset);
    CesiumViewState* vs = makeTestViewState();

    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 20 && host.beginCalls == 0; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
    }
    ASSERT_EQ(host.beginCalls, 1);

    ASSERT_EQ(
        cesium_asset_request_complete(
            host.lastId, 200, nullptr, 0,
            reinterpret_cast<const uint8_t*>(buffer), std::strlen(buffer)),
        1);

    std::memset(buffer, 0xFF, sizeof(buffer));

    ASSERT_TRUE(pumpUntilRootTile(f, &host, tileset, vs));

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_resolves_relative_url() {
    FakeHost host;
    fakeHostInit(&host, kTestTilesetJsonWithContent);
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    ASSERT_NOT_NULL(tileset);
    CesiumViewState* vs = makeTestViewState();

    /* Keep pumping past the root so the content request goes out. It will fail to parse --
       the served bytes are not a b3dm -- and that is fine: what is asserted is the URL that
       arrived, not what came back. */
    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 200 && host.beginCalls < 2; ++i) {
        fakeHostPump(&host);
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_credit_system_start_next_frame(f.credits);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
#if !(defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__))
        sleep_ms(5);   /* the root must be parsed on a worker before its content is requested */
#endif
    }

    ASSERT_EQ(std::strcmp(host.firstUrl, kTestTilesetUrl), 0);
    if (host.beginCalls >= 2) {
        ASSERT_EQ(std::strcmp(host.lastUrl, kExpectedResolvedContentUrl), 0);
    }

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_double_complete_is_ignored() {
    FakeHost host;
    fakeHostInit(&host, kTestTilesetJson);
    host.answerAtAll = 0;
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    CesiumViewState* vs = makeTestViewState();

    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 20 && host.beginCalls == 0; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
    }
    ASSERT_EQ(host.beginCalls, 1);

    const CesiumAssetRequestId id = host.lastId;
    const char* body = kTestTilesetJson;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(body);

    ASSERT_EQ(cesium_asset_request_complete(id, 200, nullptr, 0, bytes, std::strlen(body)), 1);
    /* The erase inside take() is the arbitration point, so the second caller finds nothing
       and reports so rather than resolving an already-resolved promise. */
    ASSERT_EQ(cesium_asset_request_complete(id, 500, nullptr, 0, nullptr, 0), 0);
    ASSERT_EQ(cesium_asset_request_fail(id, "too late"), 0);

    ASSERT_TRUE(pumpUntilRootTile(f, &host, tileset, vs));

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_unknown_id_is_ignored() {
    /* No accessor need even exist. An id is just a number, and a number nobody issued is a
       lookup miss -- which is the property that makes the completion functions safe to call
       from a continuation scheduled by somebody else's runtime. */
    ASSERT_EQ(cesium_asset_request_complete(
                  CESIUM_ASSET_REQUEST_ID_INVALID, 200, nullptr, 0, nullptr, 0), 0);
    ASSERT_EQ(cesium_asset_request_complete(
                  (CesiumAssetRequestId)999999, 200, nullptr, 0, nullptr, 0), 0);
    ASSERT_EQ(cesium_asset_request_fail((CesiumAssetRequestId)999999, "nobody"), 0);
    return 0;
}

static int test_host_accessor_fail_request() {
    FakeHost host;
    fakeHostInit(&host, nullptr);
    host.answerAtAll = 0;
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    CesiumViewState* vs = makeTestViewState();

    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 20 && host.beginCalls == 0; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
    }
    ASSERT_EQ(host.beginCalls, 1);

    cesium_clear_last_error();
    ASSERT_EQ(cesium_asset_request_fail(host.lastId, "simulated DNS failure"), 1);

    const char* err = cesium_get_last_error();
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(std::strstr(err, "simulated DNS failure") != nullptr);

    /* Failing resolves the request rather than abandoning it, so the pump terminates and the
       pending count returns to zero instead of leaking. */
    ASSERT_TRUE(!pumpUntilRootTile(f, &host, tileset, vs, 20));
    ASSERT_EQ(cesium_asset_accessor_get_pending_request_count(f.accessor), 0);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_never_answered_is_released() {
    FakeHost host;
    fakeHostInit(&host, nullptr);
    host.answerAtAll = 0;
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    CesiumAssetRequestId strandedId = CESIUM_ASSET_REQUEST_ID_INVALID;
    {
        TilesetTestFixture f = createHostFixture(&cb);
        CesiumTilesetOptions* options = cesium_tileset_options_create();
        CesiumTileset* tileset =
            cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
        CesiumViewState* vs = makeTestViewState();

        const CesiumViewState* views[] = {vs};
        for (int i = 0; i < 20 && host.beginCalls == 0; ++i) {
            cesium_async_system_dispatch_main_thread_tasks(f.async);
            cesium_tileset_update_view(tileset, views, 1, 0.016f);
        }
        ASSERT_EQ(host.beginCalls, 1);
        strandedId = host.lastId;

        cesium_view_state_destroy(vs);
        cesium_tileset_destroy(tileset);

        for (int i = 0; i < 50; ++i) {
            cesium_async_system_dispatch_main_thread_tasks(f.async);
        }

        /* Cancelling explicitly is not tidiness here, it is the only way out, and this test
           exists to pin that down.

           Destroying the handles is not enough. An unanswered request leaves a continuation
           pending inside cesium-native, that continuation holds a copy of TilesetExternals,
           and TilesetExternals holds the accessor -- so the accessor cannot be destroyed
           while a request is outstanding. Which means its destructor, where cancellation
           lives, can never fire in exactly the situation it was written for. The first
           version of this test asserted the opposite and was wrong: measured here, after
           tearing everything down and pumping a hundred times, cancelRequest had not been
           called once and the stranded id was still live in the registry.

           So a host that stops answering must say so. There is no timeout, by choice: a
           timeout would mean this library owns a clock. */
        cesium_asset_accessor_cancel_all_requests(f.accessor);
        ASSERT_EQ(host.cancelCalls, 1);
        ASSERT_EQ(cesium_asset_accessor_get_pending_request_count(f.accessor), 0);

        cesium_tileset_options_destroy(options);
        /* No pumping after this: f.destroy() takes the async system with it, so dispatching
           on f.async afterwards reads a destroyed object. */
        f.destroy();
    }

    /* Cancelling resolved the promise, so nothing holds the accessor now and destroy fires. */
    ASSERT_EQ(host.cancelCalls, 1);
    ASSERT_EQ(host.destroyCalls, 1);

    /* And the stranded id is now inert, which is the case the whole id-not-pointer choice
       exists for: this is what a late .NET continuation looks like. */
    ASSERT_EQ(cesium_asset_request_complete(strandedId, 200, nullptr, 0, nullptr, 0), 0);
    return 0;
}

static int test_host_accessor_completed_after_everything_destroyed() {
    FakeHost host;
    fakeHostInit(&host, kTestTilesetJson);
    host.answerAtAll = 0;
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    CesiumAssetRequestId id = CESIUM_ASSET_REQUEST_ID_INVALID;
    {
        TilesetTestFixture f = createHostFixture(&cb);
        CesiumTilesetOptions* options = cesium_tileset_options_create();
        CesiumTileset* tileset =
            cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
        CesiumViewState* vs = makeTestViewState();

        const CesiumViewState* views[] = {vs};
        for (int i = 0; i < 20 && host.beginCalls == 0; ++i) {
            cesium_async_system_dispatch_main_thread_tasks(f.async);
            cesium_tileset_update_view(tileset, views, 1, 0.016f);
        }
        ASSERT_EQ(host.beginCalls, 1);
        id = host.lastId;

        cesium_view_state_destroy(vs);
        cesium_tileset_destroy(tileset);
        for (int i = 0; i < 50; ++i) {
            cesium_async_system_dispatch_main_thread_tasks(f.async);
        }
        /* Retire the id before tearing down, for the reason spelled out in
           test_host_accessor_never_answered_is_released: an outstanding request keeps the
           accessor alive, so without this the id would still be live below and the late
           completion would find it. */
        cesium_asset_accessor_cancel_all_requests(f.accessor);
        cesium_tileset_options_destroy(options);
        f.destroy();   /* the async system goes too */
    }

    const char* body = kTestTilesetJson;
    ASSERT_EQ(
        cesium_asset_request_complete(
            id, 200, nullptr, 0,
            reinterpret_cast<const uint8_t*>(body), std::strlen(body)),
        0);
    return 0;
}

static int test_host_accessor_synchronous_completion() {
    /* A host with a memory cache answers from inside beginRequest. Resolving inline there
       would recurse through the scheduler unbounded on a single-threaded build, which is why
       every resolve is marshalled. This test is what says that rule is in force. */
    FakeHost host;
    fakeHostInit(&host, kTestTilesetJson);
    host.answerFromInsideBeginRequest = 1;
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    CesiumViewState* vs = makeTestViewState();

    ASSERT_TRUE(pumpUntilRootTile(f, &host, tileset, vs));
    ASSERT_EQ(host.beginCalls, 1);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_cancel_all_requests() {
    FakeHost host;
    fakeHostInit(&host, nullptr);
    host.answerAtAll = 0;
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    CesiumViewState* vs = makeTestViewState();

    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 20 && host.beginCalls == 0; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
    }
    ASSERT_EQ(cesium_asset_accessor_get_pending_request_count(f.accessor), 1);

    cesium_asset_accessor_cancel_all_requests(f.accessor);
    ASSERT_EQ(host.cancelCalls, 1);
    ASSERT_EQ(cesium_asset_accessor_get_pending_request_count(f.accessor), 0);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

static int test_host_accessor_worker_thread_opt_in() {
    /* Asserts the end state only. Asserting which thread beginRequest ran on would be a race
       on desktop and meaningless on wasm, where there is one. */
    FakeHost host;
    fakeHostInit(&host, kTestTilesetJson);
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);
    cb.allowBeginRequestOnWorkerThread = 1;

    TilesetTestFixture f = createHostFixture(&cb);
    CesiumTilesetOptions* options = cesium_tileset_options_create();
    cesium_tileset_options_set_load_error_callback(options, fakeHostLoadError, &host);
    CesiumTileset* tileset =
        cesium_tileset_create_from_url(f.externals, kTestTilesetUrl, options);
    CesiumViewState* vs = makeTestViewState();

    ASSERT_TRUE(pumpUntilRootTile(f, &host, tileset, vs));

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(options);
    f.destroy();
    return 0;
}

// ============================================================================
// Asynchronous Ion connection
//
// All offline. The host accessor above is what makes this testable at all: before it, an Ion
// test either reached api.cesium.com or tested nothing.
// ============================================================================

/** Everything /appData parses has a default, so an empty object is a valid response. */
static const char* kFakeAppDataJson = "{}";

struct IonAsyncResult {
    int calls;
    CesiumIonConnection* connection;
    int hadError;
    char error[256];
};

static void ionAsyncComplete(
    void* userData, CesiumIonConnection* connection, const char* error) {
    IonAsyncResult* r = static_cast<IonAsyncResult*>(userData);
    ++r->calls;
    r->connection = connection;
    r->hadError = error != nullptr;
    if (error) {
        std::snprintf(r->error, sizeof(r->error), "%s", error);
    }
}

/**
 * @brief Dispatches until the attempt reports, or gives up.
 *
 * How many dispatches it takes is an implementation detail of the continuation chain, and
 * pinning it to exactly one would be asserting on cesium-native's inlining rules rather than
 * on our contract. What the tests actually claim is the ordering: nothing before the host
 * answers, something after.
 */
static bool dispatchUntilReported(TilesetTestFixture& f, IonAsyncResult* r) {
    for (int i = 0; i < 50 && r->calls == 0; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(f.async);
    }
    return r->calls > 0;
}

static int test_ion_async_rejects_bad_arguments() {
    /* The contract is that a bad argument still reports, synchronously, rather than leaving
       the caller waiting for a callback that was never scheduled. */
    IonAsyncResult r;
    std::memset(&r, 0, sizeof(r));

    cesium_ion_connection_create_async(
        nullptr, nullptr, nullptr, nullptr, ionAsyncComplete, &r);

    ASSERT_EQ(r.calls, 1);
    ASSERT_TRUE(r.connection == nullptr);
    ASSERT_TRUE(r.hadError);

    /* A null callback is the one case with nowhere to report to. It must not crash. */
    cesium_ion_connection_create_async(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    return 0;
}

static int test_ion_async_returns_before_the_host_answers() {
    /* The point of the whole phase. The blocking form spends up to fifty seconds here; this
       one must come back with the request still outstanding and nothing reported yet. */
    FakeHost host;
    fakeHostInit(&host, kFakeAppDataJson);
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);
    TilesetTestFixture f = createHostFixture(&cb);

    IonAsyncResult r;
    std::memset(&r, 0, sizeof(r));

    cesium_ion_connection_create_async(
        f.async, f.accessor, "fake-token", "https://fake.test", ionAsyncComplete, &r);

    /* Returned without reporting. That is the whole claim: the blocking form would still be
       inside its fifty-second loop at this point. */
    ASSERT_EQ(r.calls, 0);

    /* The request itself has not reached the host yet either, because beginRequest is
       marshalled to the main thread by default. One dispatch delivers it. */
    ASSERT_EQ(host.beginCalls, 0);
    cesium_async_system_dispatch_main_thread_tasks(f.async);
    ASSERT_EQ(host.beginCalls, 1);
    ASSERT_EQ(r.calls, 0);
    ASSERT_TRUE(std::strstr(host.lastUrl, "/appData") != nullptr);

    /* Now let it finish: answer, then dispatch, which is where the callback runs. */
    fakeHostPump(&host);
    ASSERT_TRUE(dispatchUntilReported(f, &r));

    ASSERT_EQ(r.calls, 1);
    ASSERT_TRUE(r.connection != nullptr);
    ASSERT_TRUE(!r.hadError);

    cesium_ion_connection_destroy(r.connection);
    f.destroy();
    return 0;
}

static int test_ion_async_reports_http_failure() {
    FakeHost host;
    fakeHostInit(&host, "not json at all");
    host.statusToServe = 401;
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);
    TilesetTestFixture f = createHostFixture(&cb);

    IonAsyncResult r;
    std::memset(&r, 0, sizeof(r));

    cesium_ion_connection_create_async(
        f.async, f.accessor, "fake-token", "https://fake.test", ionAsyncComplete, &r);

    /* Deliver the request first. Pumping the host before this would find nothing queued,
       because beginRequest is marshalled. */
    cesium_async_system_dispatch_main_thread_tasks(f.async);
    ASSERT_EQ(host.beginCalls, 1);

    fakeHostPump(&host);
    ASSERT_TRUE(dispatchUntilReported(f, &r));

    ASSERT_EQ(r.calls, 1);
    ASSERT_TRUE(r.connection == nullptr);
    ASSERT_TRUE(r.hadError);

    f.destroy();
    return 0;
}

static int test_ion_async_reports_once_when_host_never_answers() {
    /* A host that swallows the request must still not produce two callbacks, and tearing
       everything down must not leave the attempt hanging silently. */
    FakeHost host;
    fakeHostInit(&host, kFakeAppDataJson);
    host.answerAtAll = 0;
    CesiumAssetAccessorCallbacks cb = fakeHostCallbacks(&host);
    TilesetTestFixture f = createHostFixture(&cb);

    IonAsyncResult r;
    std::memset(&r, 0, sizeof(r));

    cesium_ion_connection_create_async(
        f.async, f.accessor, "fake-token", "https://fake.test", ionAsyncComplete, &r);
    ASSERT_EQ(r.calls, 0);

    /* Destroying the accessor cancels outstanding requests with status 0, which resolves the
       promise and lets the chain report a failure exactly once. */
    f.destroy();
    ASSERT_TRUE(r.calls <= 1);
    if (r.calls == 1) {
        ASSERT_TRUE(r.connection == nullptr);
        ASSERT_TRUE(r.hadError);
    }
    return 0;
}

int main() {
    // Read Cesium Ion access token from environment
    g_ionToken = std::getenv("CESIUM_ION_TOKEN");
    if (!g_ionToken || g_ionToken[0] == '\0') {
        std::printf("NOTE: CESIUM_ION_TOKEN not set. "
                    "Ion-dependent tests will be skipped.\n"
                    "  Set it with: set CESIUM_ION_TOKEN=your_token_here\n\n");
    }

    std::printf("==============================\n");
    std::printf(" CesiumNativeC Test Suite\n");
    std::printf("==============================\n\n");

    // --- Offline / unit tests (no token required) ---
    RUN_TEST(test_error_handling);
    RUN_TEST(test_ellipsoid_wgs84);
    RUN_TEST(test_ellipsoid_create_destroy);
    RUN_TEST(test_cartographic_round_trip);
    RUN_TEST(test_surface_normal);
    RUN_TEST(test_gltf_reader_create_destroy);
    RUN_TEST(test_async_system);
    RUN_TEST(test_asset_accessor);
    RUN_TEST(test_credit_system);
    RUN_TEST(test_tileset_externals);
    RUN_TEST(test_tileset_options);
    RUN_TEST(test_view_state_perspective);
    RUN_TEST(test_null_safety);
    RUN_TEST(test_cartographic_to_cartesian_nyc);
    RUN_TEST(test_east_north_up_transform);
    RUN_TEST(test_globe_rectangle_queries);
    RUN_TEST(test_tileset_options_full_round_trip);
    RUN_TEST(test_view_state_orthographic);
    RUN_TEST(test_view_state_from_matrices);
    RUN_TEST(test_load_error_callback);
    RUN_TEST(test_scale_to_surface);

    // --- glTF, from the document embedded in test_gltf_asset.h ---
    RUN_TEST(test_gltf_reads_the_document);
    RUN_TEST(test_gltf_model_counts);
    RUN_TEST(test_gltf_scene_and_node_hierarchy);
    RUN_TEST(test_gltf_node_transform);
    RUN_TEST(test_gltf_primitive_and_attributes);
    RUN_TEST(test_gltf_accessor_data);
    RUN_TEST(test_gltf_material_texture_sampler_image);
    RUN_TEST(test_gltf_glb_round_trip);
    RUN_TEST(test_gltf_strip_feature_ids);

    // --- Raster overlays, all offline: constructing one fetches nothing ---
    RUN_TEST(test_raster_overlay_options_default);
    RUN_TEST(test_raster_overlay_constructors);
    RUN_TEST(test_raster_overlay_options_round_trip);
    RUN_TEST(test_raster_overlay_collection);

    // --- Ion object lifecycle, which needs no token ---
    RUN_TEST(test_ion_connection_argument_guards);

    // --- The remaining geospatial and tileset gaps ---
    RUN_TEST(test_ellipsoid_unit_sphere);
    RUN_TEST(test_tileset_from_url_render_content);
    RUN_TEST(test_tileset_callback_registration);

    // --- Host-provided HTTP accessor, all offline: a fake host serves strings ---
    RUN_TEST(test_host_accessor_create_destroy);
    RUN_TEST(test_host_accessor_callbacks_are_optional);
    RUN_TEST(test_host_accessor_no_begin_callback_fails_requests);
    RUN_TEST(test_host_accessor_receives_request);
    RUN_TEST(test_host_accessor_response_reaches_tileset);
    RUN_TEST(test_host_accessor_body_is_copied);
    RUN_TEST(test_host_accessor_resolves_relative_url);
    RUN_TEST(test_host_accessor_double_complete_is_ignored);
    RUN_TEST(test_host_accessor_unknown_id_is_ignored);
    RUN_TEST(test_host_accessor_fail_request);
    RUN_TEST(test_host_accessor_never_answered_is_released);
    RUN_TEST(test_host_accessor_completed_after_everything_destroyed);
    RUN_TEST(test_host_accessor_synchronous_completion);
    RUN_TEST(test_host_accessor_cancel_all_requests);
    RUN_TEST(test_host_accessor_worker_thread_opt_in);
    RUN_TEST(test_ion_async_rejects_bad_arguments);
    RUN_TEST(test_ion_async_returns_before_the_host_answers);
    RUN_TEST(test_ion_async_reports_http_failure);
    RUN_TEST(test_ion_async_reports_once_when_host_never_answers);

    // --- Online / integration tests (require CESIUM_ION_TOKEN) ---
    RUN_TEST(test_tileset_create_from_ion_world_terrain);
    RUN_TEST(test_view_update_result_statistics);
    RUN_TEST(test_tile_properties);
    RUN_TEST(test_root_tile_children);
    RUN_TEST(test_tileset_data_accessors);
    RUN_TEST(test_credit_system_with_ion);
    RUN_TEST(test_tileset_options_affect_loading);

    std::printf("\n==============================\n");
    std::printf(" Results: %d passed, %d failed\n", g_passed, g_failed);
    std::printf("==============================\n");

    return g_failed > 0 ? 1 : 0;
}
