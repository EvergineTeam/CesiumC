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

#include <variant>

CCallbackRendererResources::CCallbackRendererResources(
    const CesiumRendererResourceCallbacks& callbacks)
    : _callbacks(callbacks) {}

CCallbackRendererResources::CCallbackRendererResources()
    : _callbacks{} {}

void CCallbackRendererResources::setCallbacks(
    const CesiumRendererResourceCallbacks& callbacks) {
    _callbacks = callbacks;
}

void CCallbackRendererResources::clearCallbacks() {
    _callbacks = {};
}

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
CCallbackRendererResources::prepareInLoadThread(
    const CesiumAsync::AsyncSystem& asyncSystem,
    Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
    const glm::dmat4& transform,
    const std::any& /*rendererOptions*/)
{
    void* pResources = nullptr;

    if (_callbacks.prepareInLoadThread) {
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
    if (_callbacks.prepareInMainThread) {
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
    if (_callbacks.freeResources) {
        auto* cTile = reinterpret_cast<const CesiumTile*>(&tile);
        _callbacks.freeResources(
            _callbacks.userData, cTile, pLoadThreadResult, pMainThreadResult);
    }
}

void* CCallbackRendererResources::prepareRasterInLoadThread(
    CesiumGltf::ImageAsset& image,
    const std::any& /*rendererOptions*/)
{
    if (_callbacks.prepareRasterInLoadThread) {
        // CesiumImageMipPosition is layout-compatible with ImageAssetMipPosition,
        // so the mip array can be passed through without copying.
        static_assert(
            sizeof(CesiumImageMipPosition) ==
                sizeof(CesiumGltf::ImageAssetMipPosition),
            "CesiumImageMipPosition must match ImageAssetMipPosition layout");
        static_assert(
            static_cast<int>(CesiumGltf::GpuCompressedPixelFormat::NONE) ==
                CESIUM_GPU_COMPRESSED_PIXEL_FORMAT_NONE,
            "GpuCompressedPixelFormat enum mapping drifted");
        static_assert(
            static_cast<int>(
                CesiumGltf::GpuCompressedPixelFormat::ETC2_EAC_RG11) ==
                CESIUM_GPU_COMPRESSED_PIXEL_FORMAT_ETC2_EAC_RG11,
            "GpuCompressedPixelFormat enum mapping drifted");

        const auto& pixelData = image.pixelData;
        return _callbacks.prepareRasterInLoadThread(
            _callbacks.userData,
            reinterpret_cast<const uint8_t*>(pixelData.data()),
            image.sizeBytes < 0 ? pixelData.size() : static_cast<size_t>(image.sizeBytes), 
            image.width,
            image.height,
            image.channels,
            image.bytesPerChannel,
            static_cast<CesiumGpuCompressedPixelFormat>(image.compressedPixelFormat),
            reinterpret_cast<const CesiumImageMipPosition*>(image.mipPositions.data()),
            static_cast<int32_t>(image.mipPositions.size()));
    }
    return nullptr;
}

void* CCallbackRendererResources::prepareRasterInMainThread(
    CesiumRasterOverlays::RasterOverlayTile& /*rasterTile*/,
    void* pLoadThreadResult)
{
    if (_callbacks.prepareRasterInMainThread) {
        return _callbacks.prepareRasterInMainThread(
            _callbacks.userData, pLoadThreadResult);
    }
    return pLoadThreadResult;
}

void CCallbackRendererResources::freeRaster(
    const CesiumRasterOverlays::RasterOverlayTile& /*rasterTile*/,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept
{
    if (_callbacks.freeRasterResources) {
        _callbacks.freeRasterResources(_callbacks.userData, pLoadThreadResult, pMainThreadResult);
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
    if (_callbacks.attachRasterInMainThread) {
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
    if (_callbacks.detachRasterInMainThread) {
        auto* cTile = reinterpret_cast<const CesiumTile*>(&tile);
        _callbacks.detachRasterInMainThread(
            _callbacks.userData,
            cTile,
            overlayTextureCoordinateID,
            pMainThreadRendererResources);
    }
}
