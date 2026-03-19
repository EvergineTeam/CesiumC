cmake_minimum_required(VERSION 3.15)
project(CesiumNativeC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required on Linux/macOS: static libs linked into our shared lib need PIC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Hide internal symbols by default on Unix; only CESIUM_API symbols are exported.
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Build cesium-native as a subdirectory.
# Disable tests (not needed for our wrapper).
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
        CesiumGltfWriter
        CesiumJsonWriter
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

# ---------- Test executable ----------

enable_testing()

add_executable(CesiumNativeCTests tests/test_cesium_native_c.cpp)
target_link_libraries(CesiumNativeCTests PRIVATE CesiumNativeC)
set_target_properties(CesiumNativeCTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME CesiumNativeCTests COMMAND CesiumNativeCTests)
