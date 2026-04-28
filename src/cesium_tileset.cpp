/**
 * @file cesium_tileset.cpp
 * @brief C wrapper for Cesium3DTilesSelection::Tileset, TilesetExternals, and
 *        ViewUpdateResult accessors.
 */

#include "cesium_internal.h"
#include "cesium_renderer_resources.h"

#include <cesium/cesium_tileset.h>

#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesSelection/ViewUpdateResult.h>
#include "cesium_wrappers.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumUtility/CreditSystem.h>

#include <memory>
#include <string>
#include <vector>

// ---- Helper casts ----

static AsyncSystemWrapper* asAsync(CesiumAsyncSystem* h) {
    return reinterpret_cast<AsyncSystemWrapper*>(h);
}

static AssetAccessorWrapper* asAccessor(CesiumAssetAccessor* h) {
    return reinterpret_cast<AssetAccessorWrapper*>(h);
}

static CreditSystemWrapper* asCredits(CesiumCreditSystem* h) {
    return reinterpret_cast<CreditSystemWrapper*>(h);
}

static ExternalsWrapper* asExternals(CesiumTilesetExternals* h) {
    return reinterpret_cast<ExternalsWrapper*>(h);
}

static TilesetWrapper* asTileset(CesiumTileset* h) {
    return reinterpret_cast<TilesetWrapper*>(h);
}

static const TilesetWrapper* asTileset(const CesiumTileset* h) {
    return reinterpret_cast<const TilesetWrapper*>(h);
}

using Cesium3DTilesSelection::TilesetOptions;
using Cesium3DTilesSelection::ViewState;
using Cesium3DTilesSelection::ViewUpdateResult;

extern "C" {

// ============================================================================
// TilesetExternals
// ============================================================================

/**
 * @note The caller must keep asyncSystem, accessor, and creditSystem alive
 * for the entire lifetime of any Tileset created with these externals.
 */
CESIUM_API CesiumTilesetExternals* cesium_tileset_externals_create(
    CesiumAsyncSystem* asyncSystem,
    CesiumAssetAccessor* accessor,
    CesiumCreditSystem* creditSystem)
{
    if (!asyncSystem || !accessor || !creditSystem) return nullptr;
    CESIUM_TRY_BEGIN

    auto pRenderer = std::make_shared<CCallbackRendererResources>();

    Cesium3DTilesSelection::TilesetExternals ext{
        asAccessor(accessor)->pAccessor,
        pRenderer,
        asAsync(asyncSystem)->asyncSystem,
        asCredits(creditSystem)->pCreditSystem
    };

    auto* wrapper = new ExternalsWrapper{std::move(ext), pRenderer};
    return reinterpret_cast<CesiumTilesetExternals*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_tileset_externals_destroy(CesiumTilesetExternals* externals) {
    if (!externals) return;
    delete asExternals(externals);
}

CESIUM_API void cesium_tileset_externals_set_renderer_resource_callbacks(
    CesiumTilesetExternals* externals,
    const CesiumRendererResourceCallbacks* callbacks)
{
    if (!externals) return;
    auto* wrapper = asExternals(externals);
    if (callbacks) {
        wrapper->pRendererResources->setCallbacks(*callbacks);
    } else {
        wrapper->pRendererResources->clearCallbacks();
    }
}

// ============================================================================
// Tileset
// ============================================================================

CESIUM_API CesiumTileset* cesium_tileset_create_from_url(
    CesiumTilesetExternals* externals,
    const char* url,
    const CesiumTilesetOptions* options)
{
    if (!externals || !url) return nullptr;
    CESIUM_TRY_BEGIN
    auto* ext = asExternals(externals);

    TilesetOptions opts;
    if (options) {
        opts = *reinterpret_cast<const TilesetOptions*>(options);
    }

    auto* wrapper = new TilesetWrapper{
        std::make_unique<Cesium3DTilesSelection::Tileset>(
            ext->externals, std::string(url), opts)
    };
    return reinterpret_cast<CesiumTileset*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API CesiumTileset* cesium_tileset_create_from_ion(
    CesiumTilesetExternals* externals,
    int64_t ionAssetID,
    const char* ionAccessToken,
    const CesiumTilesetOptions* options,
    const char* ionAssetEndpointUrl)
{
    if (!externals || !ionAccessToken) return nullptr;
    CESIUM_TRY_BEGIN
    auto* ext = asExternals(externals);

    TilesetOptions opts;
    if (options) {
        opts = *reinterpret_cast<const TilesetOptions*>(options);
    }

    std::string endpointUrl = ionAssetEndpointUrl
        ? std::string(ionAssetEndpointUrl)
        : std::string("https://api.cesium.com/");

    auto* wrapper = new TilesetWrapper{
        std::make_unique<Cesium3DTilesSelection::Tileset>(
            ext->externals,
            ionAssetID,
            std::string(ionAccessToken),
            opts,
            endpointUrl)
    };
    return reinterpret_cast<CesiumTileset*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_tileset_destroy(CesiumTileset* tileset) {
    if (!tileset) return;
    delete asTileset(tileset);
}

CESIUM_API const CesiumViewUpdateResult* cesium_tileset_update_view(
    CesiumTileset* tileset,
    const CesiumViewState* const* viewStates,
    int viewStateCount,
    float deltaTime)
{
    if (!tileset || !viewStates || viewStateCount <= 0) return nullptr;
    CESIUM_TRY_BEGIN
    auto* wrapper = asTileset(tileset);
    auto* ts = wrapper->pTileset.get();

    std::vector<ViewState> frustums;
    frustums.reserve(static_cast<size_t>(viewStateCount));
    for (int i = 0; i < viewStateCount; ++i) {
        if (viewStates[i]) {
            frustums.push_back(
                *reinterpret_cast<const ViewState*>(viewStates[i]));
        }
    }

    auto& viewGroup = ts->getDefaultViewGroup();
    const auto& result = ts->updateViewGroup(viewGroup, frustums, deltaTime);
    ts->loadTiles();

    return reinterpret_cast<const CesiumViewUpdateResult*>(&result);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API const CesiumTile* cesium_tileset_get_root_tile(const CesiumTileset* tileset) {
    if (!tileset) return nullptr;
    CESIUM_TRY_BEGIN
    const auto* ts = asTileset(tileset)->pTileset.get();
    const auto* root = ts->getRootTile();
    return reinterpret_cast<const CesiumTile*>(root);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API int cesium_tileset_is_root_tile_available(const CesiumTileset* tileset) {
    if (!tileset) return 0;
    CESIUM_TRY_BEGIN
    return asTileset(tileset)->pTileset->getRootTile() != nullptr ? 1 : 0;
    CESIUM_TRY_END
    return 0;
}

CESIUM_API void cesium_tileset_set_root_tile_available_callback(
    CesiumTileset* tileset,
    CesiumRootTileAvailableCallback callback,
    void* userData)
{
    if (!tileset) return;
    CESIUM_TRY_BEGIN
    auto* wrapper = asTileset(tileset);

    // Update the stored callback (NULL clears it)
    wrapper->rootTileCallback = callback;
    wrapper->rootTileCallbackUserData = userData;

    // Register the event forwarder only once
    if (!wrapper->rootTileEventRegistered) {
        wrapper->rootTileEventRegistered = true;
        wrapper->pTileset->getRootTileAvailableEvent().thenInMainThread(
            [wrapper]() {
                if (wrapper->rootTileCallback) {
                    wrapper->rootTileCallback(wrapper->rootTileCallbackUserData);
                }
            });
    }
    CESIUM_TRY_END
}

CESIUM_API float cesium_tileset_compute_load_progress(CesiumTileset* tileset) {
    if (!tileset) return 0.0f;
    CESIUM_TRY_BEGIN
    return asTileset(tileset)->pTileset->computeLoadProgress();
    CESIUM_TRY_END
    return 0.0f;
}

CESIUM_API int32_t cesium_tileset_get_number_of_tiles_loaded(const CesiumTileset* tileset) {
    if (!tileset) return 0;
    CESIUM_TRY_BEGIN
    return asTileset(tileset)->pTileset->getNumberOfTilesLoaded();
    CESIUM_TRY_END
    return 0;
}

CESIUM_API int64_t cesium_tileset_get_total_data_bytes(const CesiumTileset* tileset) {
    if (!tileset) return 0;
    CESIUM_TRY_BEGIN
    return asTileset(tileset)->pTileset->getTotalDataBytes();
    CESIUM_TRY_END
    return 0;
}

// ============================================================================
// ViewUpdateResult accessors
// ============================================================================

static const ViewUpdateResult* asResult(const CesiumViewUpdateResult* h) {
    return reinterpret_cast<const ViewUpdateResult*>(h);
}

CESIUM_API int cesium_view_update_result_get_tiles_to_render_count(
    const CesiumViewUpdateResult* result)
{
    return static_cast<int>(asResult(result)->tilesToRenderThisFrame.size());
}

CESIUM_API const CesiumTile* cesium_view_update_result_get_tile_to_render(
    const CesiumViewUpdateResult* result, int index)
{
    const auto& tiles = asResult(result)->tilesToRenderThisFrame;
    return reinterpret_cast<const CesiumTile*>(tiles[static_cast<size_t>(index)].get());
}

CESIUM_API int cesium_view_update_result_get_tiles_fading_out_count(
    const CesiumViewUpdateResult* result)
{
    return static_cast<int>(asResult(result)->tilesFadingOut.size());
}

CESIUM_API const CesiumTile* cesium_view_update_result_get_tile_fading_out(
    const CesiumViewUpdateResult* result, int index)
{
    const auto& tiles = asResult(result)->tilesFadingOut;
    auto it = std::next(tiles.begin(), index);
    return reinterpret_cast<const CesiumTile*>(it->get());
}

CESIUM_API int32_t cesium_view_update_result_get_frame_number(
    const CesiumViewUpdateResult* result)
{
    return asResult(result)->frameNumber;
}

CESIUM_API uint32_t cesium_view_update_result_get_tiles_visited(
    const CesiumViewUpdateResult* result)
{
    return asResult(result)->tilesVisited;
}

CESIUM_API uint32_t cesium_view_update_result_get_tiles_culled(
    const CesiumViewUpdateResult* result)
{
    return asResult(result)->tilesCulled;
}

CESIUM_API uint32_t cesium_view_update_result_get_max_depth_visited(
    const CesiumViewUpdateResult* result)
{
    return asResult(result)->maxDepthVisited;
}

CESIUM_API int32_t cesium_view_update_result_get_worker_thread_load_queue_length(
    const CesiumViewUpdateResult* result)
{
    return asResult(result)->workerThreadTileLoadQueueLength;
}

CESIUM_API int32_t cesium_view_update_result_get_main_thread_load_queue_length(
    const CesiumViewUpdateResult* result)
{
    return asResult(result)->mainThreadTileLoadQueueLength;
}

} // extern "C"
