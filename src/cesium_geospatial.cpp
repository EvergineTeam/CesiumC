/**
 * @file cesium_geospatial.cpp
 * @brief C wrapper for CesiumGeospatial types: Ellipsoid, Cartographic,
 *        GlobeRectangle, GlobeTransforms.
 */

#include "cesium_internal.h"

#include <cesium/cesium_geospatial.h>

#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/GlobeTransforms.h>

#include <stdexcept>

using namespace CesiumGeospatial;

// ---------- Ellipsoid ----------

extern "C" {

CESIUM_API CesiumEllipsoid* cesium_ellipsoid_create(double radiusX, double radiusY, double radiusZ) {
    CESIUM_TRY_BEGIN
    auto* e = new Ellipsoid(radiusX, radiusY, radiusZ);
    return reinterpret_cast<CesiumEllipsoid*>(e);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API const CesiumEllipsoid* cesium_ellipsoid_wgs84(void) {
    return reinterpret_cast<const CesiumEllipsoid*>(&Ellipsoid::WGS84);
}

CESIUM_API const CesiumEllipsoid* cesium_ellipsoid_unit_sphere(void) {
    return reinterpret_cast<const CesiumEllipsoid*>(&Ellipsoid::UNIT_SPHERE);
}

CESIUM_API void cesium_ellipsoid_destroy(CesiumEllipsoid* ellipsoid) {
    if (!ellipsoid) return;
    // Guard against destroying singletons
    const Ellipsoid* e = asEllipsoid(ellipsoid);
    if (e == &Ellipsoid::WGS84 || e == &Ellipsoid::UNIT_SPHERE) return;
    delete asEllipsoid(ellipsoid);
}

CESIUM_API CesiumVec3 cesium_ellipsoid_get_radii(const CesiumEllipsoid* ellipsoid) {
    CESIUM_TRY_BEGIN
    return fromGlm(asEllipsoid(ellipsoid)->getRadii());
    CESIUM_TRY_END
    return CesiumVec3{0, 0, 0};
}

CESIUM_API double cesium_ellipsoid_get_maximum_radius(const CesiumEllipsoid* ellipsoid) {
    CESIUM_TRY_BEGIN
    return asEllipsoid(ellipsoid)->getMaximumRadius();
    CESIUM_TRY_END
    return 0.0;
}

CESIUM_API double cesium_ellipsoid_get_minimum_radius(const CesiumEllipsoid* ellipsoid) {
    CESIUM_TRY_BEGIN
    return asEllipsoid(ellipsoid)->getMinimumRadius();
    CESIUM_TRY_END
    return 0.0;
}

CESIUM_API CesiumVec3 cesium_ellipsoid_cartographic_to_cartesian(
    const CesiumEllipsoid* ellipsoid,
    CesiumCartographic cartographic)
{
    CESIUM_TRY_BEGIN
    glm::dvec3 result = asEllipsoid(ellipsoid)->cartographicToCartesian(toCartographic(cartographic));
    return fromGlm(result);
    CESIUM_TRY_END
    return CesiumVec3{0, 0, 0};
}

CESIUM_API int cesium_ellipsoid_cartesian_to_cartographic(
    const CesiumEllipsoid* ellipsoid,
    CesiumVec3 cartesian,
    CesiumCartographic* out_result)
{
    CESIUM_TRY_BEGIN
    auto result = asEllipsoid(ellipsoid)->cartesianToCartographic(toGlm(cartesian));
    if (result.has_value()) {
        *out_result = fromCartographic(result.value());
        return 1;
    }
    return 0;
    CESIUM_TRY_END
    return 0;
}

CESIUM_API CesiumVec3 cesium_ellipsoid_geodetic_surface_normal_cartesian(
    const CesiumEllipsoid* ellipsoid,
    CesiumVec3 cartesian)
{
    CESIUM_TRY_BEGIN
    return fromGlm(asEllipsoid(ellipsoid)->geodeticSurfaceNormal(toGlm(cartesian)));
    CESIUM_TRY_END
    return CesiumVec3{0, 0, 0};
}

CESIUM_API CesiumVec3 cesium_ellipsoid_geodetic_surface_normal_cartographic(
    const CesiumEllipsoid* ellipsoid,
    CesiumCartographic cartographic)
{
    CESIUM_TRY_BEGIN
    return fromGlm(asEllipsoid(ellipsoid)->geodeticSurfaceNormal(toCartographic(cartographic)));
    CESIUM_TRY_END
    return CesiumVec3{0, 0, 0};
}

CESIUM_API int cesium_ellipsoid_scale_to_geodetic_surface(
    const CesiumEllipsoid* ellipsoid,
    CesiumVec3 cartesian,
    CesiumVec3* out_result)
{
    CESIUM_TRY_BEGIN
    auto result = asEllipsoid(ellipsoid)->scaleToGeodeticSurface(toGlm(cartesian));
    if (result.has_value()) {
        *out_result = fromGlm(result.value());
        return 1;
    }
    return 0;
    CESIUM_TRY_END
    return 0;
}

CESIUM_API int cesium_ellipsoid_scale_to_geocentric_surface(
    const CesiumEllipsoid* ellipsoid,
    CesiumVec3 cartesian,
    CesiumVec3* out_result)
{
    CESIUM_TRY_BEGIN
    auto result = asEllipsoid(ellipsoid)->scaleToGeocentricSurface(toGlm(cartesian));
    if (result.has_value()) {
        *out_result = fromGlm(result.value());
        return 1;
    }
    return 0;
    CESIUM_TRY_END
    return 0;
}

// ---------- Cartographic helpers ----------

CESIUM_API CesiumCartographic cesium_cartographic_from_degrees(
    double longitudeDegrees,
    double latitudeDegrees,
    double heightMeters)
{
    Cartographic c = Cartographic::fromDegrees(longitudeDegrees, latitudeDegrees, heightMeters);
    return fromCartographic(c);
}

// ---------- GlobeRectangle helpers ----------

CESIUM_API CesiumGlobeRectangle cesium_globe_rectangle_from_degrees(
    double westDegrees,
    double southDegrees,
    double eastDegrees,
    double northDegrees)
{
    GlobeRectangle r = GlobeRectangle::fromDegrees(westDegrees, southDegrees, eastDegrees, northDegrees);
    return CesiumGlobeRectangle{r.getWest(), r.getSouth(), r.getEast(), r.getNorth()};
}

CESIUM_API double cesium_globe_rectangle_compute_width(CesiumGlobeRectangle rect) {
    GlobeRectangle r(rect.west, rect.south, rect.east, rect.north);
    return r.computeWidth();
}

CESIUM_API double cesium_globe_rectangle_compute_height(CesiumGlobeRectangle rect) {
    GlobeRectangle r(rect.west, rect.south, rect.east, rect.north);
    return r.computeHeight();
}

CESIUM_API CesiumCartographic cesium_globe_rectangle_compute_center(CesiumGlobeRectangle rect) {
    CESIUM_TRY_BEGIN
    GlobeRectangle r(rect.west, rect.south, rect.east, rect.north);
    return fromCartographic(r.computeCenter());
    CESIUM_TRY_END
    return CesiumCartographic{0, 0, 0};
}

CESIUM_API int cesium_globe_rectangle_contains(
    CesiumGlobeRectangle rect,
    CesiumCartographic point)
{
    CESIUM_TRY_BEGIN
    GlobeRectangle r(rect.west, rect.south, rect.east, rect.north);
    return r.contains(toCartographic(point)) ? 1 : 0;
    CESIUM_TRY_END
    return 0;
}

// ---------- GlobeTransforms ----------

CESIUM_API CesiumMat4 cesium_globe_transforms_east_north_up_to_fixed_frame(
    CesiumVec3 origin,
    const CesiumEllipsoid* ellipsoid)
{
    CESIUM_TRY_BEGIN
    const Ellipsoid& e = getEllipsoidOrDefault(ellipsoid);
    glm::dmat4 m = GlobeTransforms::eastNorthUpToFixedFrame(toGlm(origin), e);
    return fromGlmMat4(m);
    CESIUM_TRY_END
    CesiumMat4 identity{};
    return identity;
}

} // extern "C"
