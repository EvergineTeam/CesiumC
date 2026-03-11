/**
 * @file cesium_errors.cpp
 * @brief Thread-local error handling for the C API.
 */

#include <cesium/cesium_common.h>

#include <string>

static thread_local std::string g_lastError;

void cesium_set_last_error(const char* msg) {
    if (msg) {
        g_lastError = msg;
    } else {
        g_lastError.clear();
    }
}

extern "C" {

CESIUM_API const char* cesium_get_last_error(void) {
    if (g_lastError.empty()) {
        return nullptr;
    }
    return g_lastError.c_str();
}

CESIUM_API void cesium_clear_last_error(void) {
    g_lastError.clear();
}

} // extern "C"
