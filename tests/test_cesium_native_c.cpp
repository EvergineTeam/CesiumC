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
#include <cesium/cesium_tileset.h>

#include <cmath>
#include <cstdio>
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
// Test: Full pipeline — create tileset from URL (offline, will fail gracefully)
// ============================================================================

static int test_tileset_create_from_url() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create(nullptr);
    CesiumCreditSystem* credits = cesium_credit_system_create();
    CesiumTilesetExternals* ext =
        cesium_tileset_externals_create(async, accessor, credits);

    // Invalid URL — tileset will fail to load but should not crash
    CesiumTileset* tileset = cesium_tileset_create_from_url(
        ext, "http://localhost:1/nonexistent/tileset.json", nullptr);
    ASSERT_NOT_NULL(tileset);

    // Root tile should not be available yet (or ever, for a bad URL)
    // Just verify the API doesn't crash
    cesium_tileset_is_root_tile_available(tileset);

    // Do a few update cycles
    CesiumVec3 pos = {6378137.0, 0.0, 0.0};
    CesiumVec3 dir = {-1.0, 0.0, 0.0};
    CesiumVec3 up = {0.0, 0.0, 1.0};
    CesiumVec2 viewport = {800.0, 600.0};

    CesiumViewState* vs = cesium_view_state_create_perspective(
        pos, dir, up, viewport,
        60.0 * PI / 180.0, 45.0 * PI / 180.0, nullptr);

    const CesiumViewState* views[] = {vs};
    for (int i = 0; i < 3; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(async);
        cesium_credit_system_start_next_frame(credits);
        const CesiumViewUpdateResult* result =
            cesium_tileset_update_view(tileset, views, 1, 0.016f);
        // Result should be non-null even for a broken tileset
        ASSERT_NOT_NULL(result);
    }

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_externals_destroy(ext);
    cesium_credit_system_destroy(credits);
    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);
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
// Test: ViewUpdateResult statistics accessors
// ============================================================================

static int test_view_update_result_statistics() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create(nullptr);
    CesiumCreditSystem* credits = cesium_credit_system_create();
    CesiumTilesetExternals* ext =
        cesium_tileset_externals_create(async, accessor, credits);

    CesiumTileset* tileset = cesium_tileset_create_from_url(
        ext, "http://localhost:1/nonexistent/tileset.json", nullptr);
    ASSERT_NOT_NULL(tileset);

    CesiumVec3 pos = {6378137.0, 0.0, 0.0};
    CesiumVec3 dir = {-1.0, 0.0, 0.0};
    CesiumVec3 up = {0.0, 0.0, 1.0};
    CesiumVec2 viewport = {1920.0, 1080.0};
    CesiumViewState* vs = cesium_view_state_create_perspective(
        pos, dir, up, viewport,
        60.0 * PI / 180.0, 33.75 * PI / 180.0, nullptr);

    const CesiumViewState* views[] = {vs};
    cesium_async_system_dispatch_main_thread_tasks(async);
    const CesiumViewUpdateResult* result =
        cesium_tileset_update_view(tileset, views, 1, 0.016f);
    ASSERT_NOT_NULL(result);

    // All accessors should return valid values without crashing
    int renderCount = cesium_view_update_result_get_tiles_to_render_count(result);
    ASSERT_TRUE(renderCount >= 0);

    int fadingCount = cesium_view_update_result_get_tiles_fading_out_count(result);
    ASSERT_TRUE(fadingCount >= 0);

    int32_t frameNumber = cesium_view_update_result_get_frame_number(result);
    ASSERT_TRUE(frameNumber >= 0);

    uint32_t visited = cesium_view_update_result_get_tiles_visited(result);
    uint32_t culled = cesium_view_update_result_get_tiles_culled(result);
    uint32_t maxDepth = cesium_view_update_result_get_max_depth_visited(result);
    (void)visited; (void)culled; (void)maxDepth; // just verify no crash

    int32_t workerQueue =
        cesium_view_update_result_get_worker_thread_load_queue_length(result);
    int32_t mainQueue =
        cesium_view_update_result_get_main_thread_load_queue_length(result);
    ASSERT_TRUE(workerQueue >= 0);
    ASSERT_TRUE(mainQueue >= 0);

    // For a broken URL, no tiles should be ready to render
    ASSERT_EQ(renderCount, 0);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_externals_destroy(ext);
    cesium_credit_system_destroy(credits);
    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);
    return 0;
}

// ============================================================================
// Test: Tileset create from Ion (bad token — exercises the Ion path)
// ============================================================================

static int test_tileset_create_from_ion() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create(nullptr);
    CesiumCreditSystem* credits = cesium_credit_system_create();
    CesiumTilesetExternals* ext =
        cesium_tileset_externals_create(async, accessor, credits);
    CesiumTilesetOptions* opts = cesium_tileset_options_create();

    // Use an invalid token — tileset should be created but fail to load
    CesiumTileset* tileset = cesium_tileset_create_from_ion(
        ext, 1, "INVALID_TOKEN_FOR_TESTING", opts, nullptr);
    ASSERT_NOT_NULL(tileset);

    // Pump a few frames — should not crash
    CesiumVec3 pos = {6378137.0, 0.0, 0.0};
    CesiumVec3 dir = {-1.0, 0.0, 0.0};
    CesiumVec3 up = {0.0, 0.0, 1.0};
    CesiumVec2 viewport = {800.0, 600.0};
    CesiumViewState* vs = cesium_view_state_create_perspective(
        pos, dir, up, viewport,
        60.0 * PI / 180.0, 45.0 * PI / 180.0, nullptr);
    const CesiumViewState* views[] = {vs};

    for (int i = 0; i < 5; ++i) {
        cesium_async_system_dispatch_main_thread_tasks(async);
        const CesiumViewUpdateResult* result =
            cesium_tileset_update_view(tileset, views, 1, 0.016f);
        ASSERT_NOT_NULL(result);
    }

    // Load progress should be a valid float
    float progress = cesium_tileset_compute_load_progress(tileset);
    ASSERT_TRUE(progress >= 0.0f && progress <= 100.0f);

    cesium_view_state_destroy(vs);
    cesium_tileset_destroy(tileset);
    cesium_tileset_options_destroy(opts);
    cesium_tileset_externals_destroy(ext);
    cesium_credit_system_destroy(credits);
    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);
    return 0;
}

// ============================================================================
// Test: Tileset data accessors (numberOfTilesLoaded, totalDataBytes)
// ============================================================================

static int test_tileset_data_accessors() {
    CesiumAsyncSystem* async = cesium_async_system_create();
    CesiumAssetAccessor* accessor = cesium_asset_accessor_create(nullptr);
    CesiumCreditSystem* credits = cesium_credit_system_create();
    CesiumTilesetExternals* ext =
        cesium_tileset_externals_create(async, accessor, credits);

    CesiumTileset* tileset = cesium_tileset_create_from_url(
        ext, "http://localhost:1/nonexistent/tileset.json", nullptr);
    ASSERT_NOT_NULL(tileset);

    // Before any loading, these should return 0 without crashing
    int32_t numLoaded = cesium_tileset_get_number_of_tiles_loaded(tileset);
    ASSERT_TRUE(numLoaded >= 0);

    int64_t totalBytes = cesium_tileset_get_total_data_bytes(tileset);
    ASSERT_TRUE(totalBytes >= 0);

    float progress = cesium_tileset_compute_load_progress(tileset);
    ASSERT_TRUE(progress >= 0.0f && progress <= 100.0f);

    cesium_tileset_destroy(tileset);
    cesium_tileset_externals_destroy(ext);
    cesium_credit_system_destroy(credits);
    cesium_asset_accessor_destroy(accessor);
    cesium_async_system_destroy(async);
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

int main() {
    std::printf("==============================\n");
    std::printf(" CesiumNativeC Test Suite\n");
    std::printf("==============================\n\n");

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
    RUN_TEST(test_tileset_create_from_url);
    RUN_TEST(test_null_safety);

    // New tests from Hello World patterns
    RUN_TEST(test_cartographic_to_cartesian_nyc);
    RUN_TEST(test_east_north_up_transform);
    RUN_TEST(test_globe_rectangle_queries);
    RUN_TEST(test_tileset_options_full_round_trip);
    RUN_TEST(test_view_state_orthographic);
    RUN_TEST(test_view_state_from_matrices);
    RUN_TEST(test_load_error_callback);
    RUN_TEST(test_view_update_result_statistics);
    RUN_TEST(test_tileset_create_from_ion);
    RUN_TEST(test_tileset_data_accessors);
    RUN_TEST(test_scale_to_surface);

    std::printf("\n==============================\n");
    std::printf(" Results: %d passed, %d failed\n", g_passed, g_failed);
    std::printf("==============================\n");

    return g_failed > 0 ? 1 : 0;
}
