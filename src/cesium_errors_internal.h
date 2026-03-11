/**
 * @file cesium_errors_internal.h
 * @brief Internal error-setting function, not exported.
 */

#ifndef CESIUM_ERRORS_INTERNAL_H
#define CESIUM_ERRORS_INTERNAL_H

void cesium_set_last_error(const char* msg);

/**
 * @brief Macro to wrap C++ code in a try/catch and set the last error.
 * Use in every extern "C" function body.
 */
#define CESIUM_TRY_BEGIN try {
#define CESIUM_TRY_END                                    \
    } catch (const std::exception& e) {                   \
        cesium_set_last_error(e.what());                  \
    } catch (...) {                                       \
        cesium_set_last_error("Unknown C++ exception");   \
    }

#endif /* CESIUM_ERRORS_INTERNAL_H */
