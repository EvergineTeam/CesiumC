cmake_minimum_required(VERSION 3.15)
project(CesiumNativeC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build cesium-native as a subdirectory.
# Disable tests and curl (not needed for our wrapper).
set(CESIUM_TESTS_ENABLED OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cesium-native
                 ${CMAKE_CURRENT_BINARY_DIR}/cesium-native
                 EXCLUDE_FROM_ALL)

# ---------- CesiumNativeC shared library ----------

add_library(CesiumNativeC SHARED
    src/cesium_errors.cpp
    src/cesium_geospatial.cpp
    src/cesium_gltf.cpp
    src/cesium_async.cpp
    src/cesium_asset_accessor.cpp
    src/cesium_credit_system.cpp
    src/cesium_renderer_resources.cpp
    src/cesium_tileset_options.cpp
    src/cesium_view_state.cpp
    src/cesium_tileset.cpp
    src/cesium_tile.cpp
    src/cesium_raster_overlays.cpp
    src/cesium_ion.cpp
)

target_include_directories(CesiumNativeC
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(CesiumNativeC PRIVATE CESIUM_NATIVE_C_EXPORTS)

# Link against the required cesium-native libraries (static).
target_link_libraries(CesiumNativeC
    PRIVATE
        CesiumGeospatial
        CesiumGeometry
        CesiumGltf
        CesiumGltfReader
        CesiumUtility
        Cesium3DTilesSelection
        CesiumRasterOverlays
        CesiumIonClient
        CesiumClientCommon
        CesiumCurl
        CesiumAsync
)

# On Windows, place the DLL in a predictable output directory.
set_target_properties(CesiumNativeC PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
