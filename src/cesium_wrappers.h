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
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesSelection/ViewUpdateResult.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/ITaskProcessor.h>
#include <CesiumUtility/CreditSystem.h>

#include <cesium/cesium_tileset.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Threads are available everywhere except Emscripten built without -pthread, which is how
// .NET's browser-wasm links native code. Emscripten defines __EMSCRIPTEN_PTHREADS__ only when
// -pthread is passed, so the compiler answers this question rather than the build system --
// and a wasm build that *does* enable threads keeps the threaded behaviour without anyone
// having to remember to flip a flag.
//
// Defined here rather than in cesium_async.cpp, where it started, because two translation
// units now need the same answer and two copies of a predicate is how they drift apart.
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#define CESIUMC_NO_THREADS 1
#endif

#ifdef CESIUMC_NO_THREADS
// A lock that locks nothing, for the build where there is one thread. It exists so the code
// that needs a lock elsewhere reads the same on both, rather than growing an #if around every
// critical section -- and so nothing drags pthread_create into an archive the wasm CI job
// greps for exactly that symbol.
class CNullMutex {
public:
    void lock() noexcept {}
    void unlock() noexcept {}
    bool try_lock() noexcept { return true; }
};
using CMutex = CNullMutex;
#else
using CMutex = std::mutex;
#endif

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

struct CViewUpdateResult {
    const Cesium3DTilesSelection::ViewUpdateResult* pNativeResult = nullptr;
    std::vector<Cesium3DTilesSelection::Tile::ConstPointer> tilesFadingOut;
};

struct TilesetWrapper {
    std::unique_ptr<Cesium3DTilesSelection::Tileset> pTileset;
    std::vector<Cesium3DTilesSelection::ViewState> viewStates;
    CViewUpdateResult viewUpdateResult;

    // Root-tile-available callback (single slot, updatable)
    CesiumRootTileAvailableCallback rootTileCallback = nullptr;
    void* rootTileCallbackUserData = nullptr;
    bool rootTileEventRegistered = false;
};

#endif /* CESIUM_WRAPPERS_H */
