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

# Tell cesium-native it is targeting wasm, and that curl is out. Both must be set here, before
# add_subdirectory, and neither is redundant with what upstream does for itself:
#
#   CESIUM_TARGET_WASM  upstream sets it, but only when CMAKE_TOOLCHAIN_FILE is *named*
#                       Emscripten.cmake. Under this project the toolchain file is vcpkg.cmake,
#                       which chainloads Emscripten's, so the name check never fires and every
#                       CESIUM_TARGET_WASM branch upstream wrote -- blend2d's jit feature,
#                       CesiumClientCommon's httplib server, CesiumVectorData -- would be taken
#                       the wrong way. Set from CesiumC's scope it propagates into the
#                       subdirectory correctly.
#   CESIUM_DISABLE_CURL upstream forces this on for wasm, but with a directory-scope `set` that
#                       shadows its own cache entry, so the value never reaches this file. We
#                       need it in *this* scope to decide the link line below.
#
# Reading upstream's option rather than testing EMSCRIPTEN at every site also keeps
# -DCESIUM_DISABLE_CURL=ON working on a native platform, which is a configuration upstream
# deliberately offers.
#   CESIUM_DISABLE_LIBJPEG_TURBO turbojpeg uses setjmp for its error handling, and setjmp is not
#                       linkable from a .NET wasm application in any form. WebAssembly has no
#                       setjmp, so it has to be emulated, and of the two models .NET provides
#                       neither: not emscripten_longjmp, which is an import from the JavaScript
#                       glue, and not __wasm_setjmp with its support library. Measured by
#                       linking a real .NET wasm application against the published archive: the
#                       only setjmp among its 2409 objects came from turbojpeg.c.o, and removing
#                       it left the link with no undefined symbols at all.
#
#                       JPEG is not lost. ImageDecoder falls through to stb_image, which decodes
#                       it too, only slower. The cost is decode speed on wasm and nothing else.
if(EMSCRIPTEN)
    set(CESIUM_TARGET_WASM ON)
    set(CESIUM_DISABLE_CURL ON)
    set(CESIUM_DISABLE_LIBJPEG_TURBO ON)
endif()

# Printed rather than left to be inferred. Getting a cross build wrong is quiet: the
# dependencies come out as wasm32 and the consuming project as native x64, and the first
# symptom is a find_package rejecting a package for its pointer size, which reads as a missing
# package. CMAKE_SIZEOF_VOID_P is the line that says which of the two happened, and it is not a
# cache variable, so a build log is the only place it can be read.
message(STATUS "CesiumNativeC: system=${CMAKE_SYSTEM_NAME} "
               "pointer=${CMAKE_SIZEOF_VOID_P} bytes "
               "EMSCRIPTEN=${EMSCRIPTEN} "
               "CESIUM_TARGET_WASM=${CESIUM_TARGET_WASM} "
               "CESIUM_DISABLE_CURL=${CESIUM_DISABLE_CURL} "
               "CESIUM_DISABLE_LIBJPEG_TURBO=${CESIUM_DISABLE_LIBJPEG_TURBO}")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cesium-native
                 ${CMAKE_CURRENT_BINARY_DIR}/cesium-native
                 EXCLUDE_FROM_ALL)

# ---------- CesiumNativeC shared library ----------

# STATIC on Emscripten and on iOS, for reasons that look alike and are not.
#
# A wasm "shared library" is a SIDE_MODULE, which is not how .NET's browser-wasm consumes native
# code: it links archives into the app, which is what ImGui.Net's cimgui.a does today.
#
# iOS is a platform rule rather than a linking model. An application may only load dynamic
# libraries that ship inside its own bundle as frameworks, and a .NET iOS application is compiled
# ahead of time and linked, so the archive is linked into it. Android is the counter-example
# worth keeping in mind here: it is an ordinary shared object and stays SHARED.
if(EMSCRIPTEN OR IOS)
    set(CESIUM_NATIVE_C_LIBRARY_TYPE STATIC)
else()
    set(CESIUM_NATIVE_C_LIBRARY_TYPE SHARED)
endif()

add_library(CesiumNativeC ${CESIUM_NATIVE_C_LIBRARY_TYPE}
    src/cesium_errors.cpp
    src/cesium_geospatial.cpp
    src/cesium_gltf.cpp
    src/cesium_async.cpp
    src/cesium_asset_accessor.cpp
    src/cesium_host_accessor.cpp
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
        # New in cesium-native 0.63.0. ImageAsset and Ktx2TranscodeTargets moved here
        # out of CesiumGltf, so this is not an optional addition.
        CesiumImage
        CesiumGltfReader
        CesiumGltfWriter
        CesiumGltfContent
        Cesium3DTilesContent
        CesiumJsonWriter
        CesiumUtility
        Cesium3DTilesSelection
        CesiumRasterOverlays
        CesiumIonClient
        CesiumClientCommon
        CesiumAsync
)

# CesiumCurl only where it exists. When CESIUM_DISABLE_CURL is on, cesium-native does not even
# add_subdirectory(CesiumCurl), so the target is absent and naming it here is a hard error --
# which is exactly what made the wrapper unbuildable for the browser.
#
# The same variable drives a compile definition, so the link line and the source cannot disagree
# about which accessor exists. Testing EMSCRIPTEN in the source and CESIUM_DISABLE_CURL here
# would be two decisions that happen to agree today.
if(NOT CESIUM_DISABLE_CURL)
    target_link_libraries(CesiumNativeC PRIVATE CesiumCurl)
else()
    target_compile_definitions(CesiumNativeC PRIVATE CESIUMC_NO_CURL)
endif()

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
