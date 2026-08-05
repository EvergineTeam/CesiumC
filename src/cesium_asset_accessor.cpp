/**
 * @file cesium_asset_accessor.cpp
 * @brief C wrapper for the asset accessor. CesiumCurl wherever it is available; a placeholder
 *        when it is not. CMake decides which by defining CESIUMC_NO_CURL from
 *        CESIUM_DISABLE_CURL, so this file and the link line follow one decision rather than two.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include "cesium_wrappers.h"

#include <memory>
#include <string>

#if defined(CESIUMC_NO_CURL)

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace {

// A placeholder, on purpose, and worth being explicit about rather than leaving a TODO that
// reads like an oversight.
//
// Without curl there is no HTTP transport here at all, and the honest thing is to say so at
// every call rather than to fail at construction: the handle stays real and safe to pass around,
// so a consumer that only reads local data works, and one that requests a URL learns immediately.
// Every request resolves with status 0, which is what CurlAssetAccessor itself reports for a
// transport-level failure and what callers already handle. Nothing returns empty data dressed up
// as success.
//
// The case that matters today is the browser. A real accessor there -- over emscripten_fetch, or
// as a callback into the host, which is probably the better fit for .NET, since the host already
// has an HTTP stack and its own idea of authentication -- replaces this class and nothing else.
class FailedResponse final : public CesiumAsync::IAssetResponse {
public:
  uint16_t statusCode() const override { return 0; }
  std::string contentType() const override { return {}; }
  const CesiumAsync::HttpHeaders& headers() const override { return this->_headers; }
  std::span<const std::byte> data() const override { return {}; }

private:
  CesiumAsync::HttpHeaders _headers;
};

class FailedRequest final : public CesiumAsync::IAssetRequest {
public:
  FailedRequest(std::string method, std::string url)
      : _method(std::move(method)), _url(std::move(url)) {}

  const std::string& method() const override { return this->_method; }
  const std::string& url() const override { return this->_url; }
  const CesiumAsync::HttpHeaders& headers() const override { return this->_headers; }
  const CesiumAsync::IAssetResponse* response() const override { return &this->_response; }

private:
  std::string _method;
  std::string _url;
  CesiumAsync::HttpHeaders _headers;
  FailedResponse _response;
};

class UnimplementedAssetAccessor final : public CesiumAsync::IAssetAccessor {
public:
  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> get(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string& url,
      const std::vector<THeader>& /*headers*/) override {
    return asyncSystem.createResolvedFuture<std::shared_ptr<CesiumAsync::IAssetRequest>>(
        std::make_shared<FailedRequest>("GET", url));
  }

  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string& verb,
      const std::string& url,
      const std::vector<THeader>& /*headers*/,
      const std::span<const std::byte>& /*contentPayload*/) override {
    return asyncSystem.createResolvedFuture<std::shared_ptr<CesiumAsync::IAssetRequest>>(
        std::make_shared<FailedRequest>(verb, url));
  }

  void tick() noexcept override {}
};

} // namespace

#else

#include <CesiumCurl/CurlAssetAccessor.h>

#endif

extern "C" {

CESIUM_API CesiumAssetAccessor* cesium_asset_accessor_create(const char* userAgent) {
    CESIUM_TRY_BEGIN
#if defined(CESIUMC_NO_CURL)
    (void)userAgent;
    auto* wrapper = new AssetAccessorWrapper{
        std::make_shared<UnimplementedAssetAccessor>()
    };
#else
    CesiumCurl::CurlAssetAccessorOptions options;
    if (userAgent) {
        options.userAgent = userAgent;
    }
    auto* wrapper = new AssetAccessorWrapper{
        std::make_shared<CesiumCurl::CurlAssetAccessor>(options)
    };
#endif
    return reinterpret_cast<CesiumAssetAccessor*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_asset_accessor_destroy(CesiumAssetAccessor* accessor) {
    if (!accessor) return;
    delete reinterpret_cast<AssetAccessorWrapper*>(accessor);
}

} // extern "C"
