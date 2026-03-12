/**
 * @file cesium_tileset_options.cpp
 * @brief C wrapper for Cesium3DTilesSelection::TilesetOptions.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include <Cesium3DTilesSelection/TilesetLoadFailureDetails.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>

using Cesium3DTilesSelection::TilesetOptions;

static TilesetOptions* asOptions(CesiumTilesetOptions* h) {
    return reinterpret_cast<TilesetOptions*>(h);
}

static const TilesetOptions* asOptions(const CesiumTilesetOptions* h) {
    return reinterpret_cast<const TilesetOptions*>(h);
}

extern "C" {

CESIUM_API CesiumTilesetOptions* cesium_tileset_options_create(void) {
    CESIUM_TRY_BEGIN
    auto* opts = new TilesetOptions();
    return reinterpret_cast<CesiumTilesetOptions*>(opts);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_tileset_options_destroy(CesiumTilesetOptions* options) {
    if (!options) return;
    delete asOptions(options);
}

// --- Setters ---

CESIUM_API void cesium_tileset_options_set_maximum_screen_space_error(
    CesiumTilesetOptions* options, double value) {
    if (!options) return;
    asOptions(options)->maximumScreenSpaceError = value;
}

CESIUM_API void cesium_tileset_options_set_maximum_simultaneous_tile_loads(
    CesiumTilesetOptions* options, uint32_t value) {
    if (!options) return;
    asOptions(options)->maximumSimultaneousTileLoads = value;
}

CESIUM_API void cesium_tileset_options_set_maximum_cached_bytes(
    CesiumTilesetOptions* options, int64_t value) {
    if (!options) return;
    asOptions(options)->maximumCachedBytes = value;
}

CESIUM_API void cesium_tileset_options_set_preload_ancestors(
    CesiumTilesetOptions* options, int value) {
    if (!options) return;
    asOptions(options)->preloadAncestors = (value != 0);
}

CESIUM_API void cesium_tileset_options_set_preload_siblings(
    CesiumTilesetOptions* options, int value) {
    if (!options) return;
    asOptions(options)->preloadSiblings = (value != 0);
}

CESIUM_API void cesium_tileset_options_set_forbid_holes(
    CesiumTilesetOptions* options, int value) {
    if (!options) return;
    asOptions(options)->forbidHoles = (value != 0);
}

CESIUM_API void cesium_tileset_options_set_enable_frustum_culling(
    CesiumTilesetOptions* options, int value) {
    if (!options) return;
    asOptions(options)->enableFrustumCulling = (value != 0);
}

CESIUM_API void cesium_tileset_options_set_enable_fog_culling(
    CesiumTilesetOptions* options, int value) {
    if (!options) return;
    asOptions(options)->enableFogCulling = (value != 0);
}

CESIUM_API void cesium_tileset_options_set_enable_occlusion_culling(
    CesiumTilesetOptions* options, int value) {
    if (!options) return;
    asOptions(options)->enableOcclusionCulling = (value != 0);
}

CESIUM_API void cesium_tileset_options_set_enable_lod_transition_period(
    CesiumTilesetOptions* options, int value) {
    if (!options) return;
    asOptions(options)->enableLodTransitionPeriod = (value != 0);
}

CESIUM_API void cesium_tileset_options_set_lod_transition_length(
    CesiumTilesetOptions* options, float value) {
    if (!options) return;
    asOptions(options)->lodTransitionLength = value;
}

CESIUM_API void cesium_tileset_options_set_load_error_callback(
    CesiumTilesetOptions* options,
    CesiumTilesetLoadErrorCallback callback,
    void* userData)
{
    if (!options) return;
    if (callback) {
        asOptions(options)->loadErrorCallback =
            [callback, userData](
                const Cesium3DTilesSelection::TilesetLoadFailureDetails& details) {
                callback(userData, details.message.c_str());
            };
    } else {
        asOptions(options)->loadErrorCallback = nullptr;
    }
}

// --- Getters ---

CESIUM_API double cesium_tileset_options_get_maximum_screen_space_error(
    const CesiumTilesetOptions* options) {
    if (!options) return 16.0;
    return asOptions(options)->maximumScreenSpaceError;
}

CESIUM_API uint32_t cesium_tileset_options_get_maximum_simultaneous_tile_loads(
    const CesiumTilesetOptions* options) {
    if (!options) return 20;
    return asOptions(options)->maximumSimultaneousTileLoads;
}

CESIUM_API int64_t cesium_tileset_options_get_maximum_cached_bytes(
    const CesiumTilesetOptions* options) {
    if (!options) return 512 * 1024 * 1024;
    return asOptions(options)->maximumCachedBytes;
}

CESIUM_API int cesium_tileset_options_get_preload_ancestors(
    const CesiumTilesetOptions* options) {
    if (!options) return 1;
    return asOptions(options)->preloadAncestors ? 1 : 0;
}

CESIUM_API int cesium_tileset_options_get_preload_siblings(
    const CesiumTilesetOptions* options) {
    if (!options) return 1;
    return asOptions(options)->preloadSiblings ? 1 : 0;
}

CESIUM_API int cesium_tileset_options_get_forbid_holes(
    const CesiumTilesetOptions* options) {
    if (!options) return 0;
    return asOptions(options)->forbidHoles ? 1 : 0;
}

CESIUM_API int cesium_tileset_options_get_enable_frustum_culling(
    const CesiumTilesetOptions* options) {
    if (!options) return 1;
    return asOptions(options)->enableFrustumCulling ? 1 : 0;
}

CESIUM_API int cesium_tileset_options_get_enable_fog_culling(
    const CesiumTilesetOptions* options) {
    if (!options) return 1;
    return asOptions(options)->enableFogCulling ? 1 : 0;
}

CESIUM_API int cesium_tileset_options_get_enable_occlusion_culling(
    const CesiumTilesetOptions* options) {
    if (!options) return 1;
    return asOptions(options)->enableOcclusionCulling ? 1 : 0;
}

CESIUM_API int cesium_tileset_options_get_enable_lod_transition_period(
    const CesiumTilesetOptions* options) {
    if (!options) return 0;
    return asOptions(options)->enableLodTransitionPeriod ? 1 : 0;
}

CESIUM_API float cesium_tileset_options_get_lod_transition_length(
    const CesiumTilesetOptions* options) {
    if (!options) return 0.5f;
    return asOptions(options)->lodTransitionLength;
}

} // extern "C"
