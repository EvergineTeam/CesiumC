/**
 * @file cesium_renderer_resources.h
 * @brief Internal: C++ implementation of IPrepareRendererResources that
 *        delegates to CesiumRendererResourceCallbacks function pointers.
 */

#ifndef CESIUM_RENDERER_RESOURCES_INTERNAL_H
#define CESIUM_RENDERER_RESOURCES_INTERNAL_H

#include <cesium/cesium_tileset.h>

#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumGltf/Model.h>

#include <memory>

/**
 * @brief An IPrepareRendererResources that dispatches to C function pointers.
 *
 * When a callback pointer is NULL, it acts as a no-op. This is the default
 * mode when no callbacks are registered.
 */
class CCallbackRendererResources
    : public Cesium3DTilesSelection::IPrepareRendererResources {
public:
    explicit CCallbackRendererResources(
        const CesiumRendererResourceCallbacks& callbacks);
    CCallbackRendererResources(); // no-op default

    void setCallbacks(const CesiumRendererResourceCallbacks& callbacks);
    void clearCallbacks();

    // -- IPrepareRendererResources --
    CesiumAsync::Future<
        Cesium3DTilesSelection::TileLoadResultAndRenderResources>
    prepareInLoadThread(
        const CesiumAsync::AsyncSystem& asyncSystem,
        Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
        const glm::dmat4& transform,
        const std::any& rendererOptions) override;

    void* prepareInMainThread(
        Cesium3DTilesSelection::Tile& tile,
        void* pLoadThreadResult) override;

    void free(
        Cesium3DTilesSelection::Tile& tile,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;

    // -- IPrepareRasterOverlayRendererResources --
    void* prepareRasterInLoadThread(
        CesiumGltf::ImageAsset& image,
        const std::any& rendererOptions) override;

    void* prepareRasterInMainThread(
        CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pLoadThreadResult) override;

    void freeRaster(
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;

    // -- attach / detach raster --
    void attachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pMainThreadRendererResources,
        const glm::dvec2& translation,
        const glm::dvec2& scale) override;

    void detachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pMainThreadRendererResources) noexcept override;

private:
    CesiumRendererResourceCallbacks _callbacks{};
};

#endif /* CESIUM_RENDERER_RESOURCES_INTERNAL_H */
