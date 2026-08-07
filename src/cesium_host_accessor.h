/**
 * @file cesium_host_accessor.h
 * @brief An IAssetAccessor whose transport is supplied by the host through callbacks.
 *
 * This header is NOT part of the public API.
 *
 * It exists because browser-wasm has no libcurl and the host already has a working HTTP
 * stack. It is not browser-only: any platform may use it to own networking for
 * authentication, caching or a proxy.
 *
 * The whole design turns on one thing. IAssetAccessor::get() must return a Future
 * immediately, while the host will only answer later -- so the Promise is created here,
 * parked, and resolved out of band. cesium-native supports exactly that:
 * AsyncSystem::createPromise, with CesiumNativeTests/src/FileAccessor.cpp as the shape to
 * copy. CurlAssetAccessor is not the model; it resolves by *returning* from a blocking
 * worker lambda, which cannot work where there is one thread.
 */

#ifndef CESIUM_HOST_ACCESSOR_H
#define CESIUM_HOST_ACCESSOR_H

#include "cesium_wrappers.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>

#include <cesium/cesium_tileset.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

/**
 * @brief A response built from bytes the host handed over.
 *
 * Owns its data. That is not a style choice: cesium-native keeps the shared_ptr<IAssetRequest>
 * alive through the whole load pipeline -- the glTF parse, and in the overlay path the image
 * decode -- and offers no callback announcing when the last reference drops. There is no
 * moment at which we could tell a host it may free its buffer, so we copy instead.
 */
class CHostAssetResponse final : public CesiumAsync::IAssetResponse {
public:
    CHostAssetResponse(
        uint16_t statusCode,
        CesiumAsync::HttpHeaders&& headers,
        std::vector<std::byte>&& data);

    uint16_t statusCode() const override { return this->_statusCode; }
    std::string contentType() const override { return this->_contentType; }
    const CesiumAsync::HttpHeaders& headers() const override { return this->_headers; }
    std::span<const std::byte> data() const override { return this->_data; }

private:
    uint16_t _statusCode;
    // Resolved once at construction rather than looked up per call, because upstream returns
    // it by value and the tile pipeline asks repeatedly.
    std::string _contentType;
    CesiumAsync::HttpHeaders _headers;
    std::vector<std::byte> _data;
};

/**
 * @brief The request handed back to cesium-native once the host has answered.
 *
 * IAssetRequest documents that all four accessors may be called from any thread. That is true
 * here because of one ordering rule: setResponse happens exactly once, always before the
 * promise is resolved, and nothing mutates the request afterwards. The resolve supplies the
 * release edge and the continuation reading it supplies the acquire edge, so no member needs
 * a lock.
 */
class CHostAssetRequest final : public CesiumAsync::IAssetRequest {
public:
    CHostAssetRequest(
        std::string&& method,
        std::string&& url,
        CesiumAsync::HttpHeaders&& headers);

    const std::string& method() const override { return this->_method; }
    const std::string& url() const override { return this->_url; }
    const CesiumAsync::HttpHeaders& headers() const override { return this->_headers; }
    const CesiumAsync::IAssetResponse* response() const override {
        return this->_response.get();
    }

    void setResponse(std::unique_ptr<CHostAssetResponse> response) {
        this->_response = std::move(response);
    }

private:
    std::string _method;
    std::string _url;
    CesiumAsync::HttpHeaders _headers;
    std::unique_ptr<CHostAssetResponse> _response;
};

/**
 * @brief An accessor that asks the host to perform each request.
 *
 * Held by shared_ptr in two places at once -- the C handle and, once a tileset exists,
 * TilesetExternals -- so teardown lives in the destructor rather than in
 * cesium_asset_accessor_destroy.
 */
class CHostAssetAccessor final
    : public std::enable_shared_from_this<CHostAssetAccessor>,
      public CesiumAsync::IAssetAccessor {
public:
    explicit CHostAssetAccessor(const CesiumAssetAccessorCallbacks& callbacks);
    ~CHostAssetAccessor() override;

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> get(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& url,
        const std::vector<THeader>& headers) override;

    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& verb,
        const std::string& url,
        const std::vector<THeader>& headers,
        const std::span<const std::byte>& contentPayload) override;

    void tick() noexcept override;

    /** Cancels and fails everything in flight. Also what the destructor does. */
    void cancelAllRequests();

    int32_t pendingRequestCount() const;

    /** Called by the registry when a request it owns is answered or retired. */
    void forgetRequest(CesiumAssetRequestId id);

private:
    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> start(
        const CesiumAsync::AsyncSystem& asyncSystem,
        std::string&& method,
        const std::string& url,
        const std::vector<THeader>& headers,
        const std::span<const std::byte>& body);

    CesiumAssetAccessorCallbacks _callbacks;
    mutable CMutex _mutex;
    std::vector<CesiumAssetRequestId> _inFlight;
};

/**
 * @brief Builds a response the host never sent, for the paths where there is no transport.
 *
 * Status 0 is what CurlAssetAccessor reports for a transport-level failure and what the tile
 * loaders already handle, so a synthesised failure is indistinguishable from a real one to
 * everything downstream.
 */
std::unique_ptr<CHostAssetResponse> makeFailedResponse();

/** Parses an array of CesiumHttpHeader into cesium-native's case-insensitive map. */
CesiumAsync::HttpHeaders toHttpHeaders(const CesiumHttpHeader* headers, int32_t count);

#endif /* CESIUM_HOST_ACCESSOR_H */
