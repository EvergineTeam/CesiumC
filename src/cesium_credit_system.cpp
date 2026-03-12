/**
 * @file cesium_credit_system.cpp
 * @brief C wrapper for CesiumUtility::CreditSystem.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include "cesium_wrappers.h"

#include <CesiumUtility/CreditSystem.h>

#include <memory>
#include <string>
#include <vector>

extern "C" {

CESIUM_API CesiumCreditSystem* cesium_credit_system_create(void) {
    CESIUM_TRY_BEGIN
    auto* wrapper = new CreditSystemWrapper{
        std::make_shared<CesiumUtility::CreditSystem>()
    };
    return reinterpret_cast<CesiumCreditSystem*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_credit_system_destroy(CesiumCreditSystem* creditSystem) {
    if (!creditSystem) return;
    delete reinterpret_cast<CreditSystemWrapper*>(creditSystem);
}

CESIUM_API int cesium_credit_system_get_credits_to_show_on_screen_count(
    const CesiumCreditSystem* creditSystem)
{
    if (!creditSystem) return 0;
    CESIUM_TRY_BEGIN
    const auto* wrapper =
        reinterpret_cast<const CreditSystemWrapper*>(creditSystem);
    const auto& snapshot = wrapper->pCreditSystem->getSnapshot(
        CesiumUtility::CreditFilteringMode::UniqueHtml);

    wrapper->cachedCredits.clear();
    for (const auto& credit : snapshot.currentCredits) {
        if (wrapper->pCreditSystem->shouldBeShownOnScreen(credit)) {
            wrapper->cachedCredits.push_back(
                wrapper->pCreditSystem->getHtml(credit));
        }
    }
    return static_cast<int>(wrapper->cachedCredits.size());
    CESIUM_TRY_END
    return 0;
}

CESIUM_API const char* cesium_credit_system_get_credit_to_show_on_screen(
    const CesiumCreditSystem* creditSystem,
    int index)
{
    if (!creditSystem) return nullptr;
    CESIUM_TRY_BEGIN
    const auto* wrapper =
        reinterpret_cast<const CreditSystemWrapper*>(creditSystem);
    if (index < 0 || index >= static_cast<int>(wrapper->cachedCredits.size()))
        return nullptr;
    return wrapper->cachedCredits[static_cast<size_t>(index)].c_str();
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_credit_system_start_next_frame(CesiumCreditSystem* creditSystem) {
    // The CreditSystem uses reference counting; the "start next frame"
    // concept maps to getting a fresh snapshot. The snapshot call itself
    // in getCreditsCount refreshes state. This is a no-op placeholder
    // for the conventional "start of frame" checkpoint.
    (void)creditSystem;
}

} // extern "C"
