/**
 * @file cesium_asset_accessor.cpp
 * @brief C wrapper for the asset accessor. CesiumCurl everywhere it exists; on Emscripten a
 *        placeholder, because upstream marks CesiumCurl platform=!wasm32.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include "cesium_wrappers.h"

#include <memory>
#include <string>

#if defined(__EMSCRIPTEN__)

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
// The question this build answers is whether CesiumNativeC *links* for wasm once CesiumCurl is
// out of the way. The wrapper pulls blend2d, libjpeg-turbo, sqlite3 and a dozen more in through
// cesium-native, and any one of them could be the real blocker. Writing a working
// emscripten_fetch accessor first would mean investing in networking before knowing whether the
// target can be linked at all.
//
// So every request fails immediately with status 0, which is what CurlAssetAccessor itself
// reports for a transport-level failure and what callers already handle. Nothing returns empty
// data dressed up as success. A real accessor -- over emscripten_fetch, or as a callback into
// the host, which is probably the better fit for .NET -- replaces the Emscripten half of this
// file and nothing else.
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
#if defined(__EMSCRIPTEN__)
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
