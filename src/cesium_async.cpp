/**
 * @file cesium_async.cpp
 * @brief C wrapper for CesiumAsync::AsyncSystem. A lock-free thread pool where threads
 *        exist; a queue drained from the host's frame where they do not.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include "cesium_wrappers.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/ITaskProcessor.h>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

// CESIUMC_NO_THREADS comes from cesium_wrappers.h, included above.

namespace {

#ifndef CESIUMC_NO_THREADS

// Vyukov MPMC bounded lock-free queue.
// Capacity must be a power of two.
class LockFreeQueue {
    struct Cell {
        std::atomic<size_t> sequence;
        std::function<void()> task;
    };

    std::unique_ptr<Cell[]> _cells;
    size_t _mask;
    alignas(64) std::atomic<size_t> _head{0};
    alignas(64) std::atomic<size_t> _tail{0};

public:
    explicit LockFreeQueue(size_t capacity)
        : _cells(std::make_unique<Cell[]>(capacity))
        , _mask(capacity - 1) {
        for (size_t i = 0; i < capacity; ++i)
            _cells[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool tryPush(std::function<void()>& f) {
        size_t pos = _head.load(std::memory_order_relaxed);
        for (;;) {
            auto& cell = _cells[pos & _mask];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (_head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    cell.task = std::move(f);
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // full
            } else {
                pos = _head.load(std::memory_order_relaxed);
            }
        }
    }

    bool tryPop(std::function<void()>& f) {
        size_t pos = _tail.load(std::memory_order_relaxed);
        for (;;) {
            auto& cell = _cells[pos & _mask];
            size_t seq = cell.sequence.load(std::memory_order_acquire);
            auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            if (diff == 0) {
                if (_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    f = std::move(cell.task);
                    cell.sequence.store(pos + _mask + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // empty
            } else {
                pos = _tail.load(std::memory_order_relaxed);
            }
        }
    }
};

class ThreadPoolTaskProcessor : public CTaskProcessor {
public:
    ThreadPoolTaskProcessor()
        : _queue(4096), _stop(false), _version(0) {
        const unsigned int hardwareThreads = std::thread::hardware_concurrency();
        const unsigned int threadCount = hardwareThreads > 2 ? hardwareThreads - 1 : 2;
        _workers.reserve(threadCount);
        for (unsigned int i = 0; i < threadCount; ++i)
            _workers.emplace_back(&ThreadPoolTaskProcessor::workerLoop, this);
    }

    ~ThreadPoolTaskProcessor() override {
        _stop.store(true, std::memory_order_release);
        _version.fetch_add(1, std::memory_order_release);
        _version.notify_all();
        for (auto& t : _workers)
            t.join();
    }

    void startTask(std::function<void()> f) override {
        while (!_queue.tryPush(f))
            std::this_thread::yield();
        _version.fetch_add(1, std::memory_order_release);
        _version.notify_one();
    }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> task;
            if (_queue.tryPop(task)) {
                task();
                continue;
            }
            if (_stop.load(std::memory_order_acquire))
                return;
            // Snapshot version, double-check queue, then sleep
            auto v = _version.load(std::memory_order_acquire);
            if (_queue.tryPop(task)) {
                task();
                continue;
            }
            _version.wait(v, std::memory_order_relaxed);
        }
    }

    LockFreeQueue _queue;
    std::atomic<bool> _stop;
    alignas(64) std::atomic<uint64_t> _version;
    std::vector<std::thread> _workers;
};

#else // CESIUMC_NO_THREADS

// No workers, so startTask() queues and the host's per-frame call runs the queue. See the note on
// CTaskProcessor in cesium_wrappers.h for why this defers instead of running inline.
//
// Single-threaded by construction, so no synchronisation: every method here runs on the one thread
// there is.
class DeferredTaskProcessor final : public CTaskProcessor {
public:
    void startTask(std::function<void()> f) override {
        this->_queue.push_back(std::move(f));
    }

    void drainDeferredTasks() override {
        // Swap before running, because a task may call startTask() again -- that is the normal
        // case, not an edge case, since continuations schedule more work. Running from a swapped
        // batch means the new work lands in _queue for the next round rather than invalidating
        // the container being iterated.
        //
        // Bounded rounds rather than "until empty": a chain of continuations would otherwise hold
        // the frame for as long as the chain lasts, and in a browser that is a hung tab. Leftover
        // work simply runs next frame, which is what a frame-driven scheduler is for.
        constexpr int maxRoundsPerDrain = 8;

        for (int round = 0; round < maxRoundsPerDrain && !this->_queue.empty(); ++round) {
            std::vector<std::function<void()>> batch;
            batch.swap(this->_queue);
            for (auto& task : batch) {
                task();
            }
        }
    }

private:
    std::vector<std::function<void()>> _queue;
};

#endif // CESIUMC_NO_THREADS

} // anonymous namespace

#include <Cesium3DTilesContent/registerAllTileContentTypes.h>

static std::once_flag g_contentTypesRegistered;

static AsyncSystemWrapper* createAsyncSystemWrapper() {
    std::call_once(g_contentTypesRegistered, []() {
        Cesium3DTilesContent::registerAllTileContentTypes();
    });
#ifdef CESIUMC_NO_THREADS
    std::shared_ptr<CTaskProcessor> pTP = std::make_shared<DeferredTaskProcessor>();
#else
    std::shared_ptr<CTaskProcessor> pTP = std::make_shared<ThreadPoolTaskProcessor>();
#endif
    auto* w = new AsyncSystemWrapper{pTP, CesiumAsync::AsyncSystem(pTP)};
    return w;
}

extern "C" {

CESIUM_API CesiumAsyncSystem* cesium_async_system_create(void) {
    CESIUM_TRY_BEGIN
    auto* wrapper = createAsyncSystemWrapper();
    return reinterpret_cast<CesiumAsyncSystem*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_async_system_destroy(CesiumAsyncSystem* asyncSystem) {
    if (!asyncSystem) return;
    delete reinterpret_cast<AsyncSystemWrapper*>(asyncSystem);
}

CESIUM_API void cesium_async_system_dispatch_main_thread_tasks(CesiumAsyncSystem* asyncSystem) {
    CESIUM_TRY_BEGIN
    auto* wrapper = reinterpret_cast<AsyncSystemWrapper*>(asyncSystem);

    // Deferred work first, then the main-thread queue. A background task's continuation is what
    // lands in the main-thread queue, so running them in this order makes progress within one
    // call instead of one call per stage. On a platform with threads the first line does nothing.
    wrapper->pTaskProcessor->drainDeferredTasks();
    wrapper->asyncSystem.dispatchMainThreadTasks();
    CESIUM_TRY_END
}

} // extern "C"
