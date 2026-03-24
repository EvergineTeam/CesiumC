/**
 * @file cesium_renderer_resources.cpp
 * @brief C++ implementation of IPrepareRendererResources delegating to C callbacks.
 */

#include "cesium_renderer_resources.h"
#include "cesium_internal.h"

#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>

#include <cstring>
#include <variant>

CCallbackRendererResources::CCallbackRendererResources(
    const CesiumRendererResourceCallbacks& callbacks)
    : _callbacks(callbacks), _hasCallbacks(true) {}

CCallbackRendererResources::CCallbackRendererResources()
    : _callbacks{}, _hasCallbacks(false) {
    std::memset(&_callbacks, 0, sizeof(_callbacks));
}

void CCallbackRendererResources::setCallbacks(
    const CesiumRendererResourceCallbacks& callbacks) {
    _callbacks = callbacks;
    _hasCallbacks = true;
}

void CCallbackRendererResources::clearCallbacks() {
    std::memset(&_callbacks, 0, sizeof(_callbacks));
    _hasCallbacks = false;
}

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
CCallbackRendererResources::prepareInLoadThread(
    const CesiumAsync::AsyncSystem& asyncSystem,
    Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
    const glm::dmat4& transform,
    const std::any& /*rendererOptions*/)
{
    void* pResources = nullptr;

    if (_hasCallbacks && _callbacks.prepareInLoadThread) {
        // Extract the glTF model if present
        auto* pModel = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);
        if (pModel) {
            CesiumMat4 cTransform = fromGlmMat4(transform);
            auto* cModel = reinterpret_cast<const CesiumGltfModel*>(pModel);
            pResources = _callbacks.prepareInLoadThread(
                _callbacks.userData, cModel, cTransform);
        }
    }

    return asyncSystem.createResolvedFuture(
        Cesium3DTilesSelection::TileLoadResultAndRenderResources{
            std::move(tileLoadResult), pResources});
}

void* CCallbackRendererResources::prepareInMainThread(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult)
{
    if (_hasCallbacks && _callbacks.prepareInMainThread) {
        auto* cTile = reinterpret_cast<const CesiumTile*>(&tile);
        return _callbacks.prepareInMainThread(
            _callbacks.userData, cTile, pLoadThreadResult);
    }
    return pLoadThreadResult;
}

void CCallbackRendererResources::free(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept
{
    if (_hasCallbacks && _callbacks.freeResources) {
        auto* cTile = reinterpret_cast<const CesiumTile*>(&tile);
        _callbacks.freeResources(
            _callbacks.userData, cTile, pLoadThreadResult, pMainThreadResult);
    }
}

void* CCallbackRendererResources::prepareRasterInLoadThread(
    CesiumGltf::ImageAsset& image,
    const std::any& /*rendererOptions*/)
{
    if (_hasCallbacks && _callbacks.prepareRasterInLoadThread) {
        const auto& pixelData = image.pixelData;
        return _callbacks.prepareRasterInLoadThread(
            _callbacks.userData,
            reinterpret_cast<const uint8_t*>(pixelData.data()),
            image.sizeBytes < 0 ? pixelData.size() : static_cast<size_t>(image.sizeBytes), 
            image.width,
            image.height,
            image.channels,
            image.bytesPerChannel);
    }
    return nullptr;
}

void* CCallbackRendererResources::prepareRasterInMainThread(
    CesiumRasterOverlays::RasterOverlayTile& /*rasterTile*/,
    void* pLoadThreadResult)
{
    if (_hasCallbacks && _callbacks.prepareRasterInMainThread) {
        return _callbacks.prepareRasterInMainThread(
            _callbacks.userData, pLoadThreadResult);
    }
    return pLoadThreadResult;
}

void CCallbackRendererResources::freeRaster(
    const CesiumRasterOverlays::RasterOverlayTile& /*rasterTile*/,
    void* /*pLoadThreadResult*/,
    void* pMainThreadResult) noexcept
{
    if (_hasCallbacks && _callbacks.freeRasterResources) {
        _callbacks.freeRasterResources(_callbacks.userData, pMainThreadResult);
    }
}

void CCallbackRendererResources::attachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& /*rasterTile*/,
    void* pMainThreadRendererResources,
    const glm::dvec2& translation,
    const glm::dvec2& scale)
{
    if (_hasCallbacks && _callbacks.attachRasterInMainThread) {
        auto* cTile = reinterpret_cast<const CesiumTile*>(&tile);
        _callbacks.attachRasterInMainThread(
            _callbacks.userData,
            cTile,
            overlayTextureCoordinateID,
            pMainThreadRendererResources,
            fromGlm(translation),
            fromGlm(scale));
    }
}

void CCallbackRendererResources::detachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& /*rasterTile*/,
    void* pMainThreadRendererResources) noexcept
{
    if (_hasCallbacks && _callbacks.detachRasterInMainThread) {
        auto* cTile = reinterpret_cast<const CesiumTile*>(&tile);
        _callbacks.detachRasterInMainThread(
            _callbacks.userData,
            cTile,
            overlayTextureCoordinateID,
            pMainThreadRendererResources);
    }
}
