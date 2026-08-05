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

// A task processor that may need draining from the main thread.
//
// cesium-native's ITaskProcessor promises that startTask() runs its function "in a background
// thread". Where there are threads, that is what happens and drainDeferredTasks() has nothing to
// do. Where there are none -- Emscripten built without -pthread, which is how .NET's browser-wasm
// links native code -- startTask() can only queue, and the queue has to be run from somewhere.
// That somewhere is the host's per-frame call to
// cesium_async_system_dispatch_main_thread_tasks().
//
// Running the function inline inside startTask() was the other option and it is worse: the
// scheduler calls startTask() from inside its own scope (CesiumAsync/src/TaskScheduler.cpp:30),
// so a task that schedules another task would recurse through the scheduler with no bound.
class CTaskProcessor : public CesiumAsync::ITaskProcessor {
public:
    virtual void drainDeferredTasks() {}
};

struct AsyncSystemWrapper {
    std::shared_ptr<CTaskProcessor> pTaskProcessor;
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
