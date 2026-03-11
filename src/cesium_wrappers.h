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
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumUtility/CreditSystem.h>

#include <memory>
#include <string>
#include <vector>

struct AsyncSystemWrapper {
    std::shared_ptr<CesiumAsync::ITaskProcessor> pTaskProcessor;
    CesiumAsync::AsyncSystem asyncSystem;
};

struct AssetAccessorWrapper {
    std::shared_ptr<CesiumCurl::CurlAssetAccessor> pAccessor;
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
};

#endif /* CESIUM_WRAPPERS_H */
