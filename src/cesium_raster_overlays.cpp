/**
 * @file cesium_raster_overlays.cpp
 * @brief C wrapper for CesiumRasterOverlays: IonRasterOverlay,
 *        UrlTemplateRasterOverlay, TileMapServiceRasterOverlay,
 *        WebMapServiceRasterOverlay, and RasterOverlayCollection.
 */

#include "cesium_internal.h"

#include <cesium/cesium_raster_overlays.h>

#include <Cesium3DTilesSelection/RasterOverlayCollection.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <CesiumRasterOverlays/IonRasterOverlay.h>
#include <CesiumRasterOverlays/RasterOverlay.h>
#include <CesiumRasterOverlays/TileMapServiceRasterOverlay.h>
#include <CesiumRasterOverlays/UrlTemplateRasterOverlay.h>
#include <CesiumRasterOverlays/WebMapServiceRasterOverlay.h>
#include <CesiumUtility/IntrusivePointer.h>

#include <string>

using Cesium3DTilesSelection::RasterOverlayCollection;
using CesiumRasterOverlays::IonRasterOverlay;
using CesiumRasterOverlays::RasterOverlay;
using CesiumRasterOverlays::TileMapServiceRasterOverlay;
using CesiumRasterOverlays::UrlTemplateRasterOverlay;
using CesiumRasterOverlays::WebMapServiceRasterOverlay;
using CesiumUtility::IntrusivePointer;

// Each CesiumRasterOverlay* handle stores an IntrusivePointer to keep the
// reference-counted overlay alive.
#include "cesium_wrappers.h"

struct RasterOverlayHandle {
    IntrusivePointer<RasterOverlay> pOverlay;
};

extern "C" {

// ============================================================================
// RasterOverlayCollection (from Tileset)
// ============================================================================

CESIUM_API CesiumRasterOverlayCollection* cesium_tileset_get_overlays(
    CesiumTileset* tileset)
{
    if (!tileset) return nullptr;
    CESIUM_TRY_BEGIN
    auto* wrapper = reinterpret_cast<TilesetWrapper*>(tileset);
    auto& overlays = wrapper->pTileset->getOverlays();
    return reinterpret_cast<CesiumRasterOverlayCollection*>(&overlays);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_raster_overlay_collection_add(
    CesiumRasterOverlayCollection* collection,
    CesiumRasterOverlay* overlay)
{
    if (!collection || !overlay) return;
    CESIUM_TRY_BEGIN
    auto* col = reinterpret_cast<RasterOverlayCollection*>(collection);
    auto* handle = reinterpret_cast<RasterOverlayHandle*>(overlay);
    IntrusivePointer<const RasterOverlay> pOverlay = handle->pOverlay;
    col->add(pOverlay);
    CESIUM_TRY_END
}

CESIUM_API void cesium_raster_overlay_collection_remove(
    CesiumRasterOverlayCollection* collection,
    CesiumRasterOverlay* overlay)
{
    if (!collection || !overlay) return;
    CESIUM_TRY_BEGIN
    auto* col = reinterpret_cast<RasterOverlayCollection*>(collection);
    auto* handle = reinterpret_cast<RasterOverlayHandle*>(overlay);
    IntrusivePointer<const RasterOverlay> pOverlay = handle->pOverlay;
    col->remove(pOverlay);
    CESIUM_TRY_END
}

// ============================================================================
// Ion Raster Overlay
// ============================================================================

CESIUM_API CesiumRasterOverlay* cesium_ion_raster_overlay_create(
    int64_t assetID,
    const char* accessToken,
    const char* ionAssetEndpointUrl)
{
    if (!accessToken) return nullptr;
    CESIUM_TRY_BEGIN
    std::string endpointUrl = ionAssetEndpointUrl
        ? std::string(ionAssetEndpointUrl)
        : std::string("https://api.cesium.com/");

    auto* pOverlay = new IonRasterOverlay(
        "ion-overlay",
        assetID,
        std::string(accessToken),
        {},
        endpointUrl);

    auto* handle = new RasterOverlayHandle{
        IntrusivePointer<RasterOverlay>(pOverlay)
    };
    return reinterpret_cast<CesiumRasterOverlay*>(handle);
    CESIUM_TRY_END
    return nullptr;
}

// ============================================================================
// URL Template Raster Overlay
// ============================================================================

CESIUM_API CesiumRasterOverlay* cesium_url_template_raster_overlay_create(
    const char* name,
    const char* urlTemplate,
    uint32_t minimumLevel,
    uint32_t maximumLevel,
    uint32_t tileWidth,
    uint32_t tileHeight)
{
    if (!name || !urlTemplate) return nullptr;
    CESIUM_TRY_BEGIN
    CesiumRasterOverlays::UrlTemplateRasterOverlayOptions urlOpts;
    urlOpts.minimumLevel = minimumLevel;
    urlOpts.maximumLevel = maximumLevel;
    urlOpts.tileWidth = tileWidth;
    urlOpts.tileHeight = tileHeight;

    auto* pOverlay = new UrlTemplateRasterOverlay(
        std::string(name),
        std::string(urlTemplate),
        {},
        urlOpts);

    auto* handle = new RasterOverlayHandle{
        IntrusivePointer<RasterOverlay>(pOverlay)
    };
    return reinterpret_cast<CesiumRasterOverlay*>(handle);
    CESIUM_TRY_END
    return nullptr;
}

// ============================================================================
// TMS Raster Overlay
// ============================================================================

CESIUM_API CesiumRasterOverlay* cesium_tile_map_service_raster_overlay_create(
    const char* name,
    const char* url)
{
    if (!name || !url) return nullptr;
    CESIUM_TRY_BEGIN
    auto* pOverlay = new TileMapServiceRasterOverlay(
        std::string(name),
        std::string(url));

    auto* handle = new RasterOverlayHandle{
        IntrusivePointer<RasterOverlay>(pOverlay)
    };
    return reinterpret_cast<CesiumRasterOverlay*>(handle);
    CESIUM_TRY_END
    return nullptr;
}

// ============================================================================
// WMS Raster Overlay
// ============================================================================

CESIUM_API CesiumRasterOverlay* cesium_web_map_service_raster_overlay_create(
    const char* name,
    const char* url,
    const char* layers,
    int32_t tileWidth,
    int32_t tileHeight)
{
    if (!name || !url || !layers) return nullptr;
    CESIUM_TRY_BEGIN
    CesiumRasterOverlays::WebMapServiceRasterOverlayOptions wmsOpts;
    wmsOpts.layers = std::string(layers);
    wmsOpts.tileWidth = tileWidth;
    wmsOpts.tileHeight = tileHeight;

    auto* pOverlay = new WebMapServiceRasterOverlay(
        std::string(name),
        std::string(url),
        {},
        wmsOpts);

    auto* handle = new RasterOverlayHandle{
        IntrusivePointer<RasterOverlay>(pOverlay)
    };
    return reinterpret_cast<CesiumRasterOverlay*>(handle);
    CESIUM_TRY_END
    return nullptr;
}

// ============================================================================
// RasterOverlayOptions
// ============================================================================

namespace {

// Convert between the C and C++ GPU compressed pixel format enums (1:1 order).
CesiumGpuCompressedPixelFormat toC(CesiumGltf::GpuCompressedPixelFormat f) {
    return static_cast<CesiumGpuCompressedPixelFormat>(f);
}
CesiumGltf::GpuCompressedPixelFormat toNative(CesiumGpuCompressedPixelFormat f) {
    return static_cast<CesiumGltf::GpuCompressedPixelFormat>(f);
}

void copyKtx2TargetsToC(
    const CesiumGltf::Ktx2TranscodeTargets& src,
    CesiumKtx2TranscodeTargets& dst) {
    dst.ETC1S_R = toC(src.ETC1S_R);
    dst.ETC1S_RG = toC(src.ETC1S_RG);
    dst.ETC1S_RGB = toC(src.ETC1S_RGB);
    dst.ETC1S_RGBA = toC(src.ETC1S_RGBA);
    dst.UASTC_R = toC(src.UASTC_R);
    dst.UASTC_RG = toC(src.UASTC_RG);
    dst.UASTC_RGB = toC(src.UASTC_RGB);
    dst.UASTC_RGBA = toC(src.UASTC_RGBA);
}

void copyKtx2TargetsToNative(
    const CesiumKtx2TranscodeTargets& src,
    CesiumGltf::Ktx2TranscodeTargets& dst) {
    dst.ETC1S_R = toNative(src.ETC1S_R);
    dst.ETC1S_RG = toNative(src.ETC1S_RG);
    dst.ETC1S_RGB = toNative(src.ETC1S_RGB);
    dst.ETC1S_RGBA = toNative(src.ETC1S_RGBA);
    dst.UASTC_R = toNative(src.UASTC_R);
    dst.UASTC_RG = toNative(src.UASTC_RG);
    dst.UASTC_RGB = toNative(src.UASTC_RGB);
    dst.UASTC_RGBA = toNative(src.UASTC_RGBA);
}

} // namespace

CESIUM_API void cesium_raster_overlay_options_default(
    CesiumRasterOverlayOptions* out)
{
    if (!out) return;
    CesiumRasterOverlays::RasterOverlayOptions defaults;
    out->maximumSimultaneousTileLoads = defaults.maximumSimultaneousTileLoads;
    out->subTileCacheBytes = defaults.subTileCacheBytes;
    out->maximumTextureSize = defaults.maximumTextureSize;
    out->maximumScreenSpaceError = defaults.maximumScreenSpaceError;
    out->showCreditsOnScreen = defaults.showCreditsOnScreen ? 1 : 0;
    copyKtx2TargetsToC(defaults.ktx2TranscodeTargets, out->ktx2TranscodeTargets);
}

CESIUM_API int cesium_raster_overlay_get_options(
    const CesiumRasterOverlay* overlay,
    CesiumRasterOverlayOptions* out)
{
    if (!overlay || !out) return 0;
    CESIUM_TRY_BEGIN
    const auto* handle = reinterpret_cast<const RasterOverlayHandle*>(overlay);
    const auto& opts = handle->pOverlay->getOptions();
    out->maximumSimultaneousTileLoads = opts.maximumSimultaneousTileLoads;
    out->subTileCacheBytes = opts.subTileCacheBytes;
    out->maximumTextureSize = opts.maximumTextureSize;
    out->maximumScreenSpaceError = opts.maximumScreenSpaceError;
    out->showCreditsOnScreen = opts.showCreditsOnScreen ? 1 : 0;
    copyKtx2TargetsToC(opts.ktx2TranscodeTargets, out->ktx2TranscodeTargets);
    return 1;
    CESIUM_TRY_END
    return 0;
}

CESIUM_API int cesium_raster_overlay_set_options(
    CesiumRasterOverlay* overlay,
    const CesiumRasterOverlayOptions* options)
{
    if (!overlay || !options) return 0;
    CESIUM_TRY_BEGIN
    auto* handle = reinterpret_cast<RasterOverlayHandle*>(overlay);
    auto& opts = handle->pOverlay->getOptions();
    opts.maximumSimultaneousTileLoads = options->maximumSimultaneousTileLoads;
    opts.subTileCacheBytes = options->subTileCacheBytes;
    opts.maximumTextureSize = options->maximumTextureSize;
    opts.maximumScreenSpaceError = options->maximumScreenSpaceError;
    opts.showCreditsOnScreen = options->showCreditsOnScreen != 0;
    copyKtx2TargetsToNative(options->ktx2TranscodeTargets, opts.ktx2TranscodeTargets);
    return 1;
    CESIUM_TRY_END
    return 0;
}

// ============================================================================
// Destroy
// ============================================================================

CESIUM_API void cesium_raster_overlay_destroy(CesiumRasterOverlay* overlay) {
    if (!overlay) return;
    delete reinterpret_cast<RasterOverlayHandle*>(overlay);
}

} // extern "C"
