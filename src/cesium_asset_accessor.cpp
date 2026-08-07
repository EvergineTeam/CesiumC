/**
 * @file cesium_asset_accessor.cpp
 * @brief Constructing an asset accessor. CesiumCurl where it exists; a host-callback
 *        accessor everywhere, including where it does not.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include "cesium_host_accessor.h"
#include "cesium_wrappers.h"

#include <memory>
#include <string>

#ifndef CESIUMC_NO_CURL
#include <CesiumCurl/CurlAssetAccessor.h>
#endif

extern "C" {

CESIUM_API CesiumAssetAccessor* cesium_asset_accessor_create(const char* userAgent) {
    CESIUM_TRY_BEGIN
#if defined(CESIUMC_NO_CURL)
    // No transport of our own on this platform. This is the same accessor
    // cesium_asset_accessor_create_from_callbacks returns with no host attached: every
    // request fails with status 0, and the handle stays real and safe to pass around. It
    // used to be three separate classes here doing exactly this; now it is the documented
    // no-op default of one, which is the CesiumRendererResourceCallbacks convention.
    //
    // A browser consumer installs callbacks instead. Nothing else here can help it: there is
    // no libcurl, and Emscripten's own FETCH API needs -sFETCH at a link we do not perform
    // and cannot run under Node, where these tests execute.
    (void)userAgent;
    auto* wrapper = new AssetAccessorWrapper{
        std::make_shared<CHostAssetAccessor>(CesiumAssetAccessorCallbacks{})};
#else
    CesiumCurl::CurlAssetAccessorOptions options;
    if (userAgent) {
        options.userAgent = userAgent;
    }
    auto* wrapper = new AssetAccessorWrapper{
        std::make_shared<CesiumCurl::CurlAssetAccessor>(options)};
#endif
    return reinterpret_cast<CesiumAssetAccessor*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_asset_accessor_destroy(CesiumAssetAccessor* accessor) {
    if (!accessor) return;
    // Releases this handle's reference only. A CesiumTilesetExternals holds a second one, so
    // the accessor -- and with it cancelRequest and destroy -- outlives this call whenever a
    // tileset still exists.
    delete reinterpret_cast<AssetAccessorWrapper*>(accessor);
}

} // extern "C"
