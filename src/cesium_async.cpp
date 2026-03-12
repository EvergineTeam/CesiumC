/**
 * @file cesium_async.cpp
 * @brief C wrapper for CesiumAsync::AsyncSystem with a built-in thread pool ITaskProcessor.
 */

#include "cesium_internal.h"

#include <cesium/cesium_tileset.h>

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/ITaskProcessor.h>

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace {

class ThreadPoolTaskProcessor : public CesiumAsync::ITaskProcessor {
public:
    explicit ThreadPoolTaskProcessor(
        unsigned int threadCount = std::thread::hardware_concurrency())
        : _stop(false) {
        if (threadCount == 0)
            threadCount = 2;
        _workers.reserve(threadCount);
        for (unsigned int i = 0; i < threadCount; ++i) {
            _workers.emplace_back([this]() { workerLoop(); });
        }
    }

    ~ThreadPoolTaskProcessor() override {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stop = true;
        }
        _cv.notify_all();
        for (auto& t : _workers) {
            if (t.joinable())
                t.join();
        }
    }

    void startTask(std::function<void()> f) override {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasks.push(std::move(f));
        }
        _cv.notify_one();
    }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cv.wait(lock, [this]() { return _stop || !_tasks.empty(); });
                if (_stop && _tasks.empty())
                    return;
                task = std::move(_tasks.front());
                _tasks.pop();
            }
            task();
        }
    }

    std::vector<std::thread> _workers;
    std::queue<std::function<void()>> _tasks;
    std::mutex _mutex;
    std::condition_variable _cv;
    bool _stop;
};

} // anonymous namespace

#include "cesium_wrappers.h"

static AsyncSystemWrapper* createAsyncSystemWrapper() {
    auto pTP = std::make_shared<ThreadPoolTaskProcessor>();
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
    if (!asyncSystem) return;
    CESIUM_TRY_BEGIN
    auto* wrapper = reinterpret_cast<AsyncSystemWrapper*>(asyncSystem);
    wrapper->asyncSystem.dispatchMainThreadTasks();
    CESIUM_TRY_END
}

} // extern "C"
