/**
 * @file cesium_tile.cpp
 * @brief C wrapper for read-only Tile accessors.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <CesiumGeometry/BoundingSphere.h>
#include <CesiumGeometry/OrientedBoundingBox.h>
#include <CesiumGeospatial/BoundingRegion.h>

#include <cstring>
#include <variant>

using Cesium3DTilesSelection::Tile;

static const Tile* asTile(const CesiumTile* h) {
    return reinterpret_cast<const Tile*>(h);
}

static CesiumBoundingVolume toBoundingVolume(
    const Cesium3DTilesSelection::BoundingVolume& bv)
{
    CesiumBoundingVolume result;
    std::memset(&result, 0, sizeof(result));

    if (auto* sphere =
            std::get_if<CesiumGeometry::BoundingSphere>(&bv)) {
        result.type = CESIUM_BOUNDING_VOLUME_SPHERE;
        result.volume.sphere.center = fromGlm(sphere->getCenter());
        result.volume.sphere.radius = sphere->getRadius();
    } else if (auto* obb =
                   std::get_if<CesiumGeometry::OrientedBoundingBox>(&bv)) {
        result.type = CESIUM_BOUNDING_VOLUME_ORIENTED_BOX;
        result.volume.orientedBox.center = fromGlm(obb->getCenter());
        const glm::dmat3& halfAxes = obb->getHalfAxes();
        // Store half-axes as 3 column vectors packed into 9 doubles
        std::memcpy(
            result.volume.orientedBox.halfAxes,
            &halfAxes[0][0],
            sizeof(double) * 9);
    } else if (auto* region =
                   std::get_if<CesiumGeospatial::BoundingRegion>(&bv)) {
        result.type = CESIUM_BOUNDING_VOLUME_REGION;
        const auto& rect = region->getRectangle();
        result.volume.region.rectangle.west = rect.getWest();
        result.volume.region.rectangle.south = rect.getSouth();
        result.volume.region.rectangle.east = rect.getEast();
        result.volume.region.rectangle.north = rect.getNorth();
        result.volume.region.minimumHeight = region->getMinimumHeight();
        result.volume.region.maximumHeight = region->getMaximumHeight();
    } else {
        // For other types (BoundingRegionWithLooseFittingHeights,
        // S2CellBoundingVolume, BoundingCylinderRegion), try to get a
        // BoundingSphere from the center + approximate extent.
        result.type = CESIUM_BOUNDING_VOLUME_SPHERE;
        glm::dvec3 center = Cesium3DTilesSelection::getBoundingVolumeCenter(bv);
        result.volume.sphere.center = fromGlm(center);
        result.volume.sphere.radius = 0.0;
    }

    return result;
}

extern "C" {

CESIUM_API double cesium_tile_get_geometric_error(const CesiumTile* tile) {
    return asTile(tile)->getGeometricError();
}

CESIUM_API CesiumMat4 cesium_tile_get_transform(const CesiumTile* tile) {
    return fromGlmMat4(asTile(tile)->getTransform());
}

CESIUM_API CesiumTileLoadState cesium_tile_get_load_state(const CesiumTile* tile) {
    return static_cast<CesiumTileLoadState>(static_cast<int>(asTile(tile)->getState()));
}

CESIUM_API int cesium_tile_has_render_content(const CesiumTile* tile) {
    return asTile(tile)->isRenderContent() ? 1 : 0;
}

CESIUM_API const CesiumGltfModel* cesium_tile_get_render_content_model(
    const CesiumTile* tile)
{
    const auto* renderContent = asTile(tile)->getContent().getRenderContent();
    if (!renderContent) return nullptr;
    return reinterpret_cast<const CesiumGltfModel*>(&renderContent->getModel());
}

CESIUM_API void* cesium_tile_get_render_resources(const CesiumTile* tile) {
    const auto* renderContent = asTile(tile)->getContent().getRenderContent();
    if (!renderContent) return nullptr;
    return renderContent->getRenderResources();
}

CESIUM_API int cesium_tile_get_children_count(const CesiumTile* tile) {
    return static_cast<int>(asTile(tile)->getChildren().size());
}

CESIUM_API const CesiumTile* cesium_tile_get_child(
    const CesiumTile* tile, int index)
{
    const auto& children = asTile(tile)->getChildren();
    return reinterpret_cast<const CesiumTile*>(&children[static_cast<size_t>(index)]);
}

CESIUM_API CesiumBoundingVolume cesium_tile_get_bounding_volume(
    const CesiumTile* tile)
{
    return toBoundingVolume(asTile(tile)->getBoundingVolume());
}

CESIUM_API float cesium_tile_get_lod_transition_fade_percentage(
    const CesiumTile* tile)
{
    const auto* renderContent = asTile(tile)->getContent().getRenderContent();
    if (!renderContent) return 1.0f;
    return renderContent->getLodTransitionFadePercentage();
}

} // extern "C"
