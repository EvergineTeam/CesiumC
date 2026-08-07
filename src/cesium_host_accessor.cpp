/**
 * @file cesium_host_accessor.cpp
 * @brief The host-callback asset accessor, and the registry that makes it safe.
 */

#include "cesium_host_accessor.h"

#include "cesium_errors_internal.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>

namespace {

/**
 * @brief One request handed to the host and not yet answered.
 *
 * The AsyncSystem is held **by value**, and that is load-bearing rather than convenient. It
 * is a shared_ptr to the schedulers, so a host answering after
 * cesium_async_system_destroy finds the schedulers still alive: the late resolve queues onto
 * something nobody will drain, and everything is released when the last reference drops. A
 * raw pointer here would be a use-after-free on exactly the path this design exists to make
 * safe.
 */
struct CPendingRequest {
    CesiumAsync::Promise<std::shared_ptr<CesiumAsync::IAssetRequest>> promise;
    CesiumAsync::AsyncSystem asyncSystem;
    std::shared_ptr<CHostAssetRequest> pRequest;
    std::weak_ptr<CHostAssetAccessor> pAccessor;
};

/**
 * @brief Maps a never-reused id to the request it identifies.
 *
 * Process-global on purpose. The completion functions take an id and nothing else, because
 * the host holds that id across an asynchronous round trip during which the accessor may be
 * destroyed -- and no ordering the host can impose prevents that, since its continuation is
 * scheduled by its own runtime. An accessor pointer in the completion call would dangle on a
 * path the host cannot avoid; a lookup on a retired id is a no-op.
 *
 * Two accessors therefore share one id space and one mutex. The mutex is taken once per HTTP
 * request, so contention is not a consideration.
 */
class CRequestRegistry {
public:
    /**
     * @brief The one instance, deliberately never destroyed.
     *
     * A completion arriving during static destruction at process exit finds an
     * empty-but-valid map instead of a destroyed one. Leaking a few hundred bytes at exit is
     * the cheaper half of that trade.
     */
    static CRequestRegistry& instance() {
        static CRequestRegistry* pRegistry = new CRequestRegistry();
        return *pRegistry;
    }

    CesiumAssetRequestId add(std::unique_ptr<CPendingRequest> pending) {
        std::lock_guard<CMutex> lock(this->_mutex);
        // Starts at 1: 0 is CESIUM_ASSET_REQUEST_ID_INVALID and is never issued, so a
        // zero-initialised handle in host code is always a miss rather than a hit on
        // whatever was allocated first.
        const CesiumAssetRequestId id = ++this->_nextId;
        this->_pending.emplace(id, std::move(pending));
        return id;
    }

    /**
     * @brief Removes and returns the request, or false if the id is not live.
     *
     * The erase, not the resolve, is the single arbitration point. Whoever wins it owns the
     * request; everybody else gets false and touches nothing. That is what makes a double
     * completion, a completion racing cancellation, and a completion racing accessor
     * destruction all the same harmless case.
     */
    std::unique_ptr<CPendingRequest> take(CesiumAssetRequestId id) {
        std::lock_guard<CMutex> lock(this->_mutex);
        auto it = this->_pending.find(id);
        if (it == this->_pending.end()) {
            return nullptr;
        }
        std::unique_ptr<CPendingRequest> taken = std::move(it->second);
        this->_pending.erase(it);
        return taken;
    }

private:
    CMutex _mutex;
    // unique_ptr rather than by value: CPendingRequest holds a Promise and an AsyncSystem,
    // and neither is default-constructible, so an out-parameter or a map that ever
    // value-initialises would not compile.
    std::map<CesiumAssetRequestId, std::unique_ptr<CPendingRequest>> _pending;
    CesiumAssetRequestId _nextId = 0;
};

/** Reads the content type out of already-parsed headers, or empty if absent. */
std::string contentTypeFrom(const CesiumAsync::HttpHeaders& headers) {
    const auto it = headers.find("content-type");
    return it == headers.end() ? std::string() : it->second;
}

} // namespace

CHostAssetResponse::CHostAssetResponse(
    uint16_t statusCode,
    CesiumAsync::HttpHeaders&& headers,
    std::vector<std::byte>&& data)
    : _statusCode(statusCode),
      _contentType(contentTypeFrom(headers)),
      _headers(std::move(headers)),
      _data(std::move(data)) {}

CHostAssetRequest::CHostAssetRequest(
    std::string&& method,
    std::string&& url,
    CesiumAsync::HttpHeaders&& headers)
    : _method(std::move(method)),
      _url(std::move(url)),
      _headers(std::move(headers)) {}

std::unique_ptr<CHostAssetResponse> makeFailedResponse() {
    return std::make_unique<CHostAssetResponse>(
        static_cast<uint16_t>(0),
        CesiumAsync::HttpHeaders(),
        std::vector<std::byte>());
}

CesiumAsync::HttpHeaders toHttpHeaders(const CesiumHttpHeader* headers, int32_t count) {
    CesiumAsync::HttpHeaders result;
    if (headers == nullptr || count <= 0) {
        return result;
    }
    for (int32_t i = 0; i < count; ++i) {
        // A header with no name is meaningless and a null value is not the same as an empty
        // one; skip rather than insert something a consumer would have to guess about.
        if (headers[i].name == nullptr || headers[i].value == nullptr) {
            continue;
        }
        result.insert({headers[i].name, headers[i].value});
    }
    return result;
}

CHostAssetAccessor::CHostAssetAccessor(const CesiumAssetAccessorCallbacks& callbacks)
    : _callbacks(callbacks) {}

CHostAssetAccessor::~CHostAssetAccessor() {
    this->cancelAllRequests();
    if (this->_callbacks.destroy != nullptr) {
        this->_callbacks.destroy(this->_callbacks.userData);
    }
}

void CHostAssetAccessor::forgetRequest(CesiumAssetRequestId id) {
    std::lock_guard<CMutex> lock(this->_mutex);
    this->_inFlight.erase(
        std::remove(this->_inFlight.begin(), this->_inFlight.end(), id),
        this->_inFlight.end());
}

int32_t CHostAssetAccessor::pendingRequestCount() const {
    std::lock_guard<CMutex> lock(this->_mutex);
    return static_cast<int32_t>(this->_inFlight.size());
}

void CHostAssetAccessor::cancelAllRequests() {
    // Move the list out first, so a host that completes from inside cancelRequest finds its
    // ids already retired and gets a harmless 0 rather than re-entering a locked registry.
    std::vector<CesiumAssetRequestId> ids;
    {
        std::lock_guard<CMutex> lock(this->_mutex);
        ids.swap(this->_inFlight);
    }

    // Take them out of the registry before telling the host, for the same reason.
    std::vector<std::unique_ptr<CPendingRequest>> taken;
    taken.reserve(ids.size());
    for (const CesiumAssetRequestId id : ids) {
        if (auto pending = CRequestRegistry::instance().take(id)) {
            taken.push_back(std::move(pending));
        }
    }

    // No lock held here: the host may call straight back in.
    if (this->_callbacks.cancelRequest != nullptr) {
        for (const CesiumAssetRequestId id : ids) {
            this->_callbacks.cancelRequest(this->_callbacks.userData, id);
        }
    }

    // Resolve every one of them. Async++ would reject an abandoned event_task with
    // abandoned_event_task rather than leave the chain unrun, so this is not what stands
    // between us and a hung tileset -- but a status-0 response is a far better thing for a
    // loader to report than an exception nobody named.
    for (const auto& pending : taken) {
        pending->pRequest->setResponse(makeFailedResponse());
        auto promise = pending->promise;
        auto pRequest = pending->pRequest;
        pending->asyncSystem.runInMainThread([promise, pRequest]() {
            promise.resolve(std::static_pointer_cast<CesiumAsync::IAssetRequest>(pRequest));
        });
    }
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> CHostAssetAccessor::start(
    const CesiumAsync::AsyncSystem& asyncSystem,
    std::string&& method,
    const std::string& url,
    const std::vector<THeader>& headers,
    const std::span<const std::byte>& body) {

    CesiumAsync::HttpHeaders requestHeaders(headers.begin(), headers.end());

    auto pRequest = std::make_shared<CHostAssetRequest>(
        std::move(method),
        std::string(url),
        CesiumAsync::HttpHeaders(requestHeaders));

    // No transport installed. This is the placeholder behaviour, reached through the same
    // class rather than a separate one: a real handle whose every request fails with status
    // 0, which is what a consumer reading only local data still wants.
    if (this->_callbacks.beginRequest == nullptr) {
        pRequest->setResponse(makeFailedResponse());
        return asyncSystem.createResolvedFuture(
            std::static_pointer_cast<CesiumAsync::IAssetRequest>(pRequest));
    }

    auto promise =
        asyncSystem.createPromise<std::shared_ptr<CesiumAsync::IAssetRequest>>();
    auto future = promise.getFuture();   // may only be called once

    auto pending = std::make_unique<CPendingRequest>(
        CPendingRequest{promise, asyncSystem, pRequest, this->weak_from_this()});
    const CesiumAssetRequestId id = CRequestRegistry::instance().add(std::move(pending));

    {
        std::lock_guard<CMutex> lock(this->_mutex);
        this->_inFlight.push_back(id);
    }

    // Everything the callback needs is copied into the lambda. The incoming span is not
    // guaranteed to outlive this call -- CurlAssetAccessor copies it for the same reason --
    // and the header strings must outlive the CesiumHttpHeader array that points into them.
    std::vector<std::byte> bodyCopy(body.begin(), body.end());
    auto pWeakThis = this->weak_from_this();

    auto invoke = [pWeakThis, id, pRequest, bodyCopy = std::move(bodyCopy)]() {
        auto pThis = pWeakThis.lock();
        if (!pThis) {
            return;
        }

        std::vector<CesiumHttpHeader> headerArray;
        headerArray.reserve(pRequest->headers().size());
        for (const auto& header : pRequest->headers()) {
            headerArray.push_back(
                CesiumHttpHeader{header.first.c_str(), header.second.c_str()});
        }

        pThis->_callbacks.beginRequest(
            pThis->_callbacks.userData,
            id,
            pRequest->method().c_str(),
            pRequest->url().c_str(),
            headerArray.empty() ? nullptr : headerArray.data(),
            static_cast<int32_t>(headerArray.size()),
            bodyCopy.empty() ? nullptr : reinterpret_cast<const uint8_t*>(bodyCopy.data()),
            bodyCopy.size());
    };

    if (this->_callbacks.allowBeginRequestOnWorkerThread != 0) {
        invoke();
    } else {
        // Discarding the Future is fine: Future has no [[nodiscard]] and no destructor that
        // waits. Marshalling by default means a host sees one thread on all seven platforms.
        asyncSystem.runInMainThread(std::move(invoke));
    }

    return future;
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> CHostAssetAccessor::get(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::string& url,
    const std::vector<THeader>& headers) {
    return this->start(asyncSystem, "GET", url, headers, std::span<const std::byte>());
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> CHostAssetAccessor::request(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::string& verb,
    const std::string& url,
    const std::vector<THeader>& headers,
    const std::span<const std::byte>& contentPayload) {
    return this->start(asyncSystem, std::string(verb), url, headers, contentPayload);
}

void CHostAssetAccessor::tick() noexcept {
    if (this->_callbacks.tick != nullptr) {
        this->_callbacks.tick(this->_callbacks.userData);
    }
}

namespace {

/** Shared by complete() and fail(): take the request, attach a response, resolve. */
int completeRequest(
    CesiumAssetRequestId id,
    uint16_t statusCode,
    CesiumAsync::HttpHeaders&& headers,
    std::vector<std::byte>&& body) {

    auto pending = CRequestRegistry::instance().take(id);
    if (!pending) {
        // Unknown, already answered, cancelled, or belonging to an accessor that is gone.
        // Not an error and not logged: arriving late is the normal outcome of a race the
        // host cannot win, and a log line here would teach hosts to ignore the log.
        return 0;
    }

    if (auto pAccessor = pending->pAccessor.lock()) {
        pAccessor->forgetRequest(id);
    }

    pending->pRequest->setResponse(std::make_unique<CHostAssetResponse>(
        statusCode,
        std::move(headers),
        std::move(body)));

    // Always marshalled, never resolved inline, and this is the subtle part. On a
    // single-threaded build get() runs inside a task that TaskScheduler wrapped in
    // immediate.scope(), and inside that scope continuations run inline -- so a host with a
    // memory cache completing from within beginRequest would recurse through the scheduler
    // with no bound. Marshalling removes the case instead of detecting it, and makes
    // responses arrive at one defined point on every platform.
    auto promise = pending->promise;
    auto pRequest = pending->pRequest;
    pending->asyncSystem.runInMainThread([promise, pRequest]() {
        promise.resolve(std::static_pointer_cast<CesiumAsync::IAssetRequest>(pRequest));
    });

    return 1;
}

} // namespace

extern "C" {

CESIUM_API CesiumAssetAccessor* cesium_asset_accessor_create_from_callbacks(
    const CesiumAssetAccessorCallbacks* callbacks) {
    CESIUM_TRY_BEGIN
    CesiumAssetAccessorCallbacks copy{};
    if (callbacks != nullptr) {
        copy = *callbacks;
    }
    auto* wrapper = new AssetAccessorWrapper{std::make_shared<CHostAssetAccessor>(copy)};
    return reinterpret_cast<CesiumAssetAccessor*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API int cesium_asset_request_complete(
    CesiumAssetRequestId requestId,
    uint16_t statusCode,
    const CesiumHttpHeader* headers,
    int32_t headerCount,
    const uint8_t* body,
    size_t bodySize) {
    CESIUM_TRY_BEGIN
    // The copy happens here, before this function returns, which is the contract the header
    // states: the host may free or reuse its buffer on the next line.
    std::vector<std::byte> bodyCopy;
    if (body != nullptr && bodySize > 0) {
        const auto* first = reinterpret_cast<const std::byte*>(body);
        bodyCopy.assign(first, first + bodySize);
    }
    return completeRequest(
        requestId,
        statusCode,
        toHttpHeaders(headers, headerCount),
        std::move(bodyCopy));
    CESIUM_TRY_END
    return 0;
}

CESIUM_API int cesium_asset_request_fail(
    CesiumAssetRequestId requestId,
    const char* message) {
    CESIUM_TRY_BEGIN
    if (message != nullptr) {
        // As far as it can go. IAssetResponse has no error channel, so the tileset's own
        // load error will say "status code 0" and nothing about the cause.
        cesium_set_last_error(message);
    }
    return completeRequest(
        requestId,
        static_cast<uint16_t>(0),
        CesiumAsync::HttpHeaders(),
        std::vector<std::byte>());
    CESIUM_TRY_END
    return 0;
}

CESIUM_API void cesium_asset_accessor_cancel_all_requests(CesiumAssetAccessor* accessor) {
    if (accessor == nullptr) {
        return;
    }
    CESIUM_TRY_BEGIN
    auto* wrapper = reinterpret_cast<AssetAccessorWrapper*>(accessor);
    if (auto* pHost = dynamic_cast<CHostAssetAccessor*>(wrapper->pAccessor.get())) {
        pHost->cancelAllRequests();
    }
    CESIUM_TRY_END
}

CESIUM_API int32_t cesium_asset_accessor_get_pending_request_count(
    const CesiumAssetAccessor* accessor) {
    if (accessor == nullptr) {
        return 0;
    }
    CESIUM_TRY_BEGIN
    const auto* wrapper = reinterpret_cast<const AssetAccessorWrapper*>(accessor);
    if (const auto* pHost =
            dynamic_cast<const CHostAssetAccessor*>(wrapper->pAccessor.get())) {
        return pHost->pendingRequestCount();
    }
    CESIUM_TRY_END
    return 0;
}

} // extern "C"
