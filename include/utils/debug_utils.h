/**
 * ============================================================================
 * UTILITARIOS DE DEBUG - HEADER
 * ============================================================================
 *
 * Macros e funcoes para logging e debug.
 *
 * ============================================================================
 */

#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include "esp_log.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CONFIGURACAO DE DEBUG
// ============================================================================

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 1
#endif

// ============================================================================
// MACROS DE LOG
// ============================================================================

#if DEBUG_ENABLED

#define LOG_TAG(tag) static const char* TAG = tag

#define LOG_I(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define LOG_E(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#define LOG_V(tag, fmt, ...) ESP_LOGV(tag, fmt, ##__VA_ARGS__)

// Log com funcao e linha
#define LOG_FUNC(tag) ESP_LOGI(tag, "[%s:%d]", __FUNCTION__, __LINE__)

// Log de entrada/saida de funcao
#define LOG_ENTER(tag) ESP_LOGD(tag, ">> %s", __FUNCTION__)
#define LOG_EXIT(tag)  ESP_LOGD(tag, "<< %s", __FUNCTION__)

// Log de erro com codigo
#define LOG_ERR_CODE(tag, err) ESP_LOGE(tag, "Error: %s (%d)", esp_err_to_name(err), err)

#else

#define LOG_TAG(tag)
#define LOG_I(tag, fmt, ...)
#define LOG_W(tag, fmt, ...)
#define LOG_E(tag, fmt, ...)
#define LOG_D(tag, fmt, ...)
#define LOG_V(tag, fmt, ...)
#define LOG_FUNC(tag)
#define LOG_ENTER(tag)
#define LOG_EXIT(tag)
#define LOG_ERR_CODE(tag, err)

#endif // DEBUG_ENABLED

// ============================================================================
// ASSERTS
// ============================================================================

#if DEBUG_ENABLED

#define ASSERT(cond) do { \
    if (!(cond)) { \
        ESP_LOGE("ASSERT", "Assertion failed: %s at %s:%d", #cond, __FILE__, __LINE__); \
        abort(); \
    } \
} while(0)

#define ASSERT_MSG(cond, msg) do { \
    if (!(cond)) { \
        ESP_LOGE("ASSERT", "Assertion failed: %s - %s at %s:%d", #cond, msg, __FILE__, __LINE__); \
        abort(); \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) ASSERT((ptr) != NULL)

#else

#define ASSERT(cond)
#define ASSERT_MSG(cond, msg)
#define ASSERT_NOT_NULL(ptr)

#endif // DEBUG_ENABLED

// ============================================================================
// HEAP DEBUG
// ============================================================================

/**
 * Imprime informacoes de memoria heap
 */
void debug_print_heap_info(void);

/**
 * Imprime informacoes de tasks
 */
void debug_print_task_info(void);

/**
 * Verifica integridade do heap
 * @return true se heap esta ok
 */
bool debug_check_heap(void);

#ifdef __cplusplus
}
#endif

#endif // DEBUG_UTILS_H
