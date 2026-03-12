/**
 * @file cesium_view_state.cpp
 * @brief C wrapper for Cesium3DTilesSelection::ViewState.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include <Cesium3DTilesSelection/ViewState.h>

using Cesium3DTilesSelection::ViewState;

extern "C" {

CESIUM_API CesiumViewState* cesium_view_state_create_perspective(
    CesiumVec3 position,
    CesiumVec3 direction,
    CesiumVec3 up,
    CesiumVec2 viewportSize,
    double horizontalFieldOfView,
    double verticalFieldOfView,
    const CesiumEllipsoid* ellipsoid)
{
    CESIUM_TRY_BEGIN
    auto* vs = new ViewState(
        toGlm(position),
        toGlm(direction),
        toGlm(up),
        toGlm(viewportSize),
        horizontalFieldOfView,
        verticalFieldOfView,
        getEllipsoidOrDefault(ellipsoid));
    return reinterpret_cast<CesiumViewState*>(vs);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API CesiumViewState* cesium_view_state_create_from_matrices(
    CesiumMat4 viewMatrix,
    CesiumMat4 projectionMatrix,
    CesiumVec2 viewportSize,
    const CesiumEllipsoid* ellipsoid)
{
    CESIUM_TRY_BEGIN
    auto* vs = new ViewState(
        toGlmMat4(viewMatrix),
        toGlmMat4(projectionMatrix),
        toGlm(viewportSize),
        getEllipsoidOrDefault(ellipsoid));
    return reinterpret_cast<CesiumViewState*>(vs);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API CesiumViewState* cesium_view_state_create_orthographic(
    CesiumVec3 position,
    CesiumVec3 direction,
    CesiumVec3 up,
    CesiumVec2 viewportSize,
    double left,
    double right,
    double bottom,
    double top,
    const CesiumEllipsoid* ellipsoid)
{
    CESIUM_TRY_BEGIN
    auto* vs = new ViewState(
        toGlm(position),
        toGlm(direction),
        toGlm(up),
        toGlm(viewportSize),
        left,
        right,
        bottom,
        top,
        getEllipsoidOrDefault(ellipsoid));
    return reinterpret_cast<CesiumViewState*>(vs);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_view_state_destroy(CesiumViewState* viewState) {
    if (!viewState) return;
    delete reinterpret_cast<ViewState*>(viewState);
}

} // extern "C"
