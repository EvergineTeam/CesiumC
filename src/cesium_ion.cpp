/**
 * @file cesium_ion.cpp
 * @brief C wrapper for CesiumIonClient::Connection, asset listing,
 *        and token listing.
 */

#include "cesium_internal.h"

#include <cesium/cesium_ion.h>
#include <cesium/cesium_tileset.h>

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumIonClient/ApplicationData.h>
#include <CesiumIonClient/Assets.h>
#include <CesiumIonClient/Connection.h>
#include <CesiumIonClient/Token.h>
#include <CesiumIonClient/TokenList.h>
#include "cesium_wrappers.h"

#include <CesiumUtility/Result.h>

#include <memory>
#include <string>
#include <vector>

struct IonConnectionWrapper {
    std::unique_ptr<CesiumIonClient::Connection> pConnection;
    // Keep references alive
    std::shared_ptr<CesiumAsync::IAssetAccessor> pAccessor;
};

struct IonAssetListWrapper {
    CesiumIonClient::Assets assets;
};

struct IonTokenListWrapper {
    CesiumIonClient::TokenList tokenList;
};

extern "C" {

// ============================================================================
// IonConnection
// ============================================================================

CESIUM_API CesiumIonConnection* cesium_ion_connection_create(
    CesiumAsyncSystem* asyncSystem,
    CesiumAssetAccessor* accessor,
    const char* accessToken,
    const char* apiUrl)
{
    if (!asyncSystem || !accessor || !accessToken) return nullptr;
    CESIUM_TRY_BEGIN

    auto* asyncWrapper = reinterpret_cast<AsyncSystemWrapper*>(asyncSystem);
    auto* accessorWrapper = reinterpret_cast<AssetAccessorWrapper*>(accessor);
    std::string url = apiUrl ? std::string(apiUrl) : "https://api.cesium.com";

    // We need ApplicationData for the Connection constructor.
    // Fetch it synchronously by dispatching main thread tasks.
    CesiumIonClient::ApplicationData appData;
    bool gotAppData = false;

    CesiumIonClient::Connection::appData(
        asyncWrapper->asyncSystem,
        accessorWrapper->pAccessor,
        url)
        .thenInMainThread(
            [&appData, &gotAppData](
                CesiumIonClient::Response<CesiumIonClient::ApplicationData>&&
                    response) {
                if (response.value) {
                    appData = std::move(*response.value);
                    gotAppData = true;
                }
            });

    // Pump main thread until we get the result
    for (int i = 0; i < 5000 && !gotAppData; ++i) {
        asyncWrapper->asyncSystem.dispatchMainThreadTasks();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!gotAppData) {
        cesium_set_last_error("Failed to retrieve Ion application data");
        return nullptr;
    }

    auto pConn = std::make_unique<CesiumIonClient::Connection>(
        asyncWrapper->asyncSystem,
        accessorWrapper->pAccessor,
        std::string(accessToken),
        appData,
        url);

    auto* wrapper = new IonConnectionWrapper{
        std::move(pConn), accessorWrapper->pAccessor};
    return reinterpret_cast<CesiumIonConnection*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_ion_connection_destroy(CesiumIonConnection* connection) {
    if (!connection) return;
    delete reinterpret_cast<IonConnectionWrapper*>(connection);
}

CESIUM_API void cesium_ion_connection_authorize(
    CesiumAsyncSystem* asyncSystem,
    CesiumAssetAccessor* accessor,
    const char* appID,
    const char* redirectPath,
    const char* scopes,
    CesiumIonAuthorizeUrlCallback urlCallback,
    void* urlCallbackUserData,
    CesiumIonAuthorizeCompleteCallback completeCallback,
    void* completeCallbackUserData)
{
    if (!asyncSystem || !accessor || !appID || !redirectPath || !scopes) return;
    if (!urlCallback || !completeCallback) return;
    CESIUM_TRY_BEGIN

    auto* asyncWrapper = reinterpret_cast<AsyncSystemWrapper*>(asyncSystem);
    auto* accessorWrapper = reinterpret_cast<AssetAccessorWrapper*>(accessor);

    std::string apiUrl = "https://api.cesium.com";

    // Split scopes by space
    std::vector<std::string> scopeVec;
    {
        std::string scopeStr(scopes);
        size_t pos = 0;
        while ((pos = scopeStr.find(' ')) != std::string::npos) {
            scopeVec.push_back(scopeStr.substr(0, pos));
            scopeStr = scopeStr.substr(pos + 1);
        }
        if (!scopeStr.empty()) {
            scopeVec.push_back(scopeStr);
        }
    }

    // Parse appID as a 64-bit integer without throwing. On failure, complete
    // the callback with nullptr so callers are not left waiting.
    int64_t clientID = 0;
    {
        const char* p = appID;
        if (*p == '\0') {
            completeCallback(completeCallbackUserData, nullptr);
            return;
        }

        bool negative = false;
        if (*p == '+' || *p == '-') {
            negative = (*p == '-');
            ++p;
        }

        if (*p == '\0') {
            // String was only a sign, no digits.
            completeCallback(completeCallbackUserData, nullptr);
            return;
        }

        int64_t value = 0;
        bool parseOk = true;
        for (; *p != '\0'; ++p) {
            if (*p < '0' || *p > '9') {
                parseOk = false;
                break;
            }

            int digit = *p - '0';
            // Basic overflow checks for 64-bit signed range.
            if (!negative) {
                if (value > (std::numeric_limits<int64_t>::max() - digit) / 10) {
                    parseOk = false;
                    break;
                }
                value = value * 10 + digit;
            } else {
                if (value < (std::numeric_limits<int64_t>::min() + digit) / 10) {
                    parseOk = false;
                    break;
                }
                value = value * 10 - digit;
            }
        }

        if (!parseOk) {
            completeCallback(completeCallbackUserData, nullptr);
            return;
        }

        clientID = value;
    }
    // First get app data
    CesiumIonClient::Connection::appData(
        asyncWrapper->asyncSystem, accessorWrapper->pAccessor, apiUrl)
        .thenInMainThread(
            [asyncWrapper,
             accessorWrapper,
             clientID,
             redirectPathStr = std::string(redirectPath),
             scopeVec,
             urlCallback,
             urlCallbackUserData,
             completeCallback,
             completeCallbackUserData,
             apiUrl](
                CesiumIonClient::Response<
                    CesiumIonClient::ApplicationData>&& response) {
                if (!response.value) {
                    completeCallback(completeCallbackUserData, nullptr);
                    return;
                }

                CesiumIonClient::Connection::authorize(
                    asyncWrapper->asyncSystem,
                    accessorWrapper->pAccessor,
                    "CesiumNativeC",
                    clientID,
                    redirectPathStr,
                    scopeVec,
                    [urlCallback, urlCallbackUserData](
                        const std::string& url) {
                        urlCallback(urlCallbackUserData, url.c_str());
                    },
                    *response.value,
                    apiUrl)
                    .thenInMainThread(
                        [completeCallback,
                         completeCallbackUserData,
                         accessorWrapper](
                            CesiumUtility::Result<
                                CesiumIonClient::Connection>&& result) {
                            if (result.value.has_value()) {
                                auto pConn = std::make_unique<
                                    CesiumIonClient::Connection>(
                                    std::move(*result.value));
                                auto* wrapper = new IonConnectionWrapper{
                                    std::move(pConn),
                                    accessorWrapper->pAccessor};
                                completeCallback(
                                    completeCallbackUserData,
                                    reinterpret_cast<CesiumIonConnection*>(
                                        wrapper));
                            } else {
                                completeCallback(
                                    completeCallbackUserData, nullptr);
                            }
                        });
            });
    CESIUM_TRY_END
}

// ============================================================================
// Ion Asset List
// ============================================================================

CESIUM_API void cesium_ion_connection_list_assets(
    CesiumIonConnection* connection,
    CesiumIonAssetsCompleteCallback callback,
    void* userData)
{
    if (!connection || !callback) return;
    CESIUM_TRY_BEGIN
    auto* wrapper = reinterpret_cast<IonConnectionWrapper*>(connection);

    wrapper->pConnection->assets().thenInMainThread(
        [callback, userData](
            CesiumIonClient::Response<CesiumIonClient::Assets>&& response) {
            if (response.value) {
                auto* list = new IonAssetListWrapper{std::move(*response.value)};
                callback(
                    userData,
                    reinterpret_cast<CesiumIonAssetList*>(list));
            } else {
                callback(userData, nullptr);
            }
        });
    CESIUM_TRY_END
}

CESIUM_API int cesium_ion_asset_list_get_count(const CesiumIonAssetList* list) {
    if (!list) return 0;
    auto* wrapper = reinterpret_cast<const IonAssetListWrapper*>(list);
    return static_cast<int>(wrapper->assets.items.size());
}

CESIUM_API int64_t cesium_ion_asset_list_get_asset_id(
    const CesiumIonAssetList* list, int index)
{
    if (!list) return -1;
    auto* wrapper = reinterpret_cast<const IonAssetListWrapper*>(list);
    if (index < 0 || index >= static_cast<int>(wrapper->assets.items.size()))
        return -1;
    return wrapper->assets.items[static_cast<size_t>(index)].id;
}

CESIUM_API const char* cesium_ion_asset_list_get_asset_name(
    const CesiumIonAssetList* list, int index)
{
    if (!list) return nullptr;
    auto* wrapper = reinterpret_cast<const IonAssetListWrapper*>(list);
    if (index < 0 || index >= static_cast<int>(wrapper->assets.items.size()))
        return nullptr;
    return wrapper->assets.items[static_cast<size_t>(index)].name.c_str();
}

CESIUM_API const char* cesium_ion_asset_list_get_asset_type(
    const CesiumIonAssetList* list, int index)
{
    if (!list) return nullptr;
    auto* wrapper = reinterpret_cast<const IonAssetListWrapper*>(list);
    if (index < 0 || index >= static_cast<int>(wrapper->assets.items.size()))
        return nullptr;
    return wrapper->assets.items[static_cast<size_t>(index)].type.c_str();
}

CESIUM_API void cesium_ion_asset_list_destroy(CesiumIonAssetList* list) {
    if (!list) return;
    delete reinterpret_cast<IonAssetListWrapper*>(list);
}

// ============================================================================
// Ion Token List
// ============================================================================

CESIUM_API void cesium_ion_connection_list_tokens(
    CesiumIonConnection* connection,
    CesiumIonTokensCompleteCallback callback,
    void* userData)
{
    if (!connection || !callback) return;
    CESIUM_TRY_BEGIN
    auto* wrapper = reinterpret_cast<IonConnectionWrapper*>(connection);

    wrapper->pConnection->tokens().thenInMainThread(
        [callback, userData](
            CesiumIonClient::Response<CesiumIonClient::TokenList>&& response) {
            if (response.value) {
                auto* list =
                    new IonTokenListWrapper{std::move(*response.value)};
                callback(
                    userData,
                    reinterpret_cast<CesiumIonTokenList*>(list));
            } else {
                callback(userData, nullptr);
            }
        });
    CESIUM_TRY_END
}

CESIUM_API int cesium_ion_token_list_get_count(const CesiumIonTokenList* list) {
    if (!list) return 0;
    auto* wrapper = reinterpret_cast<const IonTokenListWrapper*>(list);
    return static_cast<int>(wrapper->tokenList.items.size());
}

CESIUM_API const char* cesium_ion_token_list_get_token_name(
    const CesiumIonTokenList* list, int index)
{
    if (!list) return nullptr;
    auto* wrapper = reinterpret_cast<const IonTokenListWrapper*>(list);
    if (index < 0 || index >= static_cast<int>(wrapper->tokenList.items.size()))
        return nullptr;
    return wrapper->tokenList.items[static_cast<size_t>(index)].name.c_str();
}

CESIUM_API const char* cesium_ion_token_list_get_token_value(
    const CesiumIonTokenList* list, int index)
{
    if (!list) return nullptr;
    auto* wrapper = reinterpret_cast<const IonTokenListWrapper*>(list);
    if (index < 0 || index >= static_cast<int>(wrapper->tokenList.items.size()))
        return nullptr;
    return wrapper->tokenList.items[static_cast<size_t>(index)].token.c_str();
}

CESIUM_API void cesium_ion_token_list_destroy(CesiumIonTokenList* list) {
    if (!list) return;
    delete reinterpret_cast<IonTokenListWrapper*>(list);
}

} // extern "C"
