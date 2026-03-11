/**
 * @file cesium_internal.h
 * @brief Shared internal helpers for the CesiumNativeC implementation.
 *
 * This header is used by the .cpp files only and is NOT part of the public API.
 */

#ifndef CESIUM_INTERNAL_H
#define CESIUM_INTERNAL_H

#include "cesium_errors_internal.h"

#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cesium/cesium_common.h>
#include <cesium/cesium_geospatial.h>

#include <cstring>

// ---------- glm <-> C type conversions ----------

static inline glm::dvec2 toGlm(CesiumVec2 v) {
    return glm::dvec2(v.x, v.y);
}

static inline glm::dvec3 toGlm(CesiumVec3 v) {
    return glm::dvec3(v.x, v.y, v.z);
}

static inline glm::dmat4 toGlmMat4(const CesiumMat4& m) {
    glm::dmat4 result;
    std::memcpy(&result[0][0], m.m, sizeof(double) * 16);
    return result;
}

static inline CesiumVec2 fromGlm(const glm::dvec2& v) {
    return CesiumVec2{v.x, v.y};
}

static inline CesiumVec3 fromGlm(const glm::dvec3& v) {
    return CesiumVec3{v.x, v.y, v.z};
}

static inline CesiumMat4 fromGlmMat4(const glm::dmat4& m) {
    CesiumMat4 result;
    std::memcpy(result.m, &m[0][0], sizeof(double) * 16);
    return result;
}

// ---------- Cartographic conversions ----------

static inline CesiumGeospatial::Cartographic toCartographic(CesiumCartographic c) {
    return CesiumGeospatial::Cartographic(c.longitude, c.latitude, c.height);
}

static inline CesiumCartographic fromCartographic(const CesiumGeospatial::Cartographic& c) {
    return CesiumCartographic{c.longitude, c.latitude, c.height};
}

// ---------- Ellipsoid cast helpers ----------

static inline const CesiumGeospatial::Ellipsoid* asEllipsoid(const CesiumEllipsoid* h) {
    return reinterpret_cast<const CesiumGeospatial::Ellipsoid*>(h);
}

static inline CesiumGeospatial::Ellipsoid* asEllipsoid(CesiumEllipsoid* h) {
    return reinterpret_cast<CesiumGeospatial::Ellipsoid*>(h);
}

static inline const CesiumGeospatial::Ellipsoid& getEllipsoidOrDefault(const CesiumEllipsoid* h) {
    if (h) return *asEllipsoid(h);
    return CesiumGeospatial::Ellipsoid::WGS84;
}

#endif /* CESIUM_INTERNAL_H */
