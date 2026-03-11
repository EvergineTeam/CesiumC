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
struct RasterOverlayHandle {
    IntrusivePointer<RasterOverlay> pOverlay;
};

// ---- Tileset wrapper (must match layout in cesium_tileset.cpp) ----
struct TilesetWrapper {
    std::unique_ptr<Cesium3DTilesSelection::Tileset> pTileset;
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
// Destroy
// ============================================================================

CESIUM_API void cesium_raster_overlay_destroy(CesiumRasterOverlay* overlay) {
    if (!overlay) return;
    delete reinterpret_cast<RasterOverlayHandle*>(overlay);
}

} // extern "C"
