/**
 * @file cesium_wrappers.h
 * @brief Shared internal wrapper structs used across the CesiumNativeC
 *        implementation. All wrapper types are defined here once to avoid
 *        ODR violations from duplicating structs across translation units.
 *
 * This header is NOT part of the public API.
 */

#ifndef CESIUM_WRAPPERS_H
#define CESIUM_WRAPPERS_H

#include "cesium_renderer_resources.h"

#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/ITaskProcessor.h>
#include <CesiumUtility/CreditSystem.h>

#include <cesium/cesium_tileset.h>

#include <memory>
#include <string>
#include <vector>

struct AsyncSystemWrapper {
    std::shared_ptr<CesiumAsync::ITaskProcessor> pTaskProcessor;
    CesiumAsync::AsyncSystem asyncSystem;
};

struct AssetAccessorWrapper {
    // The interface, not the implementation. Everything downstream of this struct --
    // cesium_ion.cpp and cesium_tileset.cpp -- only ever passes pAccessor where an
    // IAssetAccessor is expected, so naming the concrete type here bought nothing and cost
    // the ability to have more than one. CesiumCurl is excluded from wasm builds upstream
    // (platform=!wasm32), which made this struct alone enough to make the wrapper
    // unbuildable for the browser.
    std::shared_ptr<CesiumAsync::IAssetAccessor> pAccessor;
};

struct CreditSystemWrapper {
    std::shared_ptr<CesiumUtility::CreditSystem> pCreditSystem;
    mutable std::vector<std::string> cachedCredits;
};

struct ExternalsWrapper {
    Cesium3DTilesSelection::TilesetExternals externals;
    std::shared_ptr<CCallbackRendererResources> pRendererResources;
};

struct TilesetWrapper {
    std::unique_ptr<Cesium3DTilesSelection::Tileset> pTileset;

    // Root-tile-available callback (single slot, updatable)
    CesiumRootTileAvailableCallback rootTileCallback = nullptr;
    void* rootTileCallbackUserData = nullptr;
    bool rootTileEventRegistered = false;
};

#endif /* CESIUM_WRAPPERS_H */
