/**
 * @file cesium_asset_accessor.cpp
 * @brief C wrapper for CesiumCurl::CurlAssetAccessor.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include <CesiumCurl/CurlAssetAccessor.h>

#include <memory>
#include <string>

struct AssetAccessorWrapper {
    std::shared_ptr<CesiumCurl::CurlAssetAccessor> pAccessor;
};

extern "C" {

CESIUM_API CesiumAssetAccessor* cesium_asset_accessor_create(const char* userAgent) {
    CESIUM_TRY_BEGIN
    CesiumCurl::CurlAssetAccessorOptions options;
    if (userAgent) {
        options.userAgent = userAgent;
    }
    auto* wrapper = new AssetAccessorWrapper{
        std::make_shared<CesiumCurl::CurlAssetAccessor>(options)
    };
    return reinterpret_cast<CesiumAssetAccessor*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_asset_accessor_destroy(CesiumAssetAccessor* accessor) {
    if (!accessor) return;
    delete reinterpret_cast<AssetAccessorWrapper*>(accessor);
}

} // extern "C"
