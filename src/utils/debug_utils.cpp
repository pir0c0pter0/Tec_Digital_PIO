/**
 * ============================================================================
 * UTILITARIOS DE DEBUG
 * ============================================================================
 */

#include "utils/debug_utils.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

LOG_TAG("DEBUG");

// ============================================================================
// HEAP DEBUG
// ============================================================================

void debug_print_heap_info(void) {
    LOG_I(TAG, "========== HEAP INFO ==========");
    LOG_I(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    LOG_I(TAG, "Min free heap: %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());

    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);

    LOG_I(TAG, "Internal heap:");
    LOG_I(TAG, "  Total free: %lu bytes", (unsigned long)info.total_free_bytes);
    LOG_I(TAG, "  Total allocated: %lu bytes", (unsigned long)info.total_allocated_bytes);
    LOG_I(TAG, "  Largest free block: %lu bytes", (unsigned long)info.largest_free_block);

    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    if (info.total_free_bytes > 0) {
        LOG_I(TAG, "PSRAM heap:");
        LOG_I(TAG, "  Total free: %lu bytes", (unsigned long)info.total_free_bytes);
        LOG_I(TAG, "  Total allocated: %lu bytes", (unsigned long)info.total_allocated_bytes);
    }

    LOG_I(TAG, "================================");
}

void debug_print_task_info(void) {
#if configUSE_TRACE_FACILITY
    LOG_I(TAG, "========== TASK INFO ==========");

    char* taskListBuffer = (char*)malloc(2048);
    if (taskListBuffer) {
        vTaskList(taskListBuffer);
        LOG_I(TAG, "Task List:\n%s", taskListBuffer);
        free(taskListBuffer);
    }

    LOG_I(TAG, "================================");
#else
    LOG_W(TAG, "Task tracing not enabled in FreeRTOS config");
#endif
}

bool debug_check_heap(void) {
    bool ok = heap_caps_check_integrity_all(true);
    if (!ok) {
        LOG_E(TAG, "HEAP CORRUPTION DETECTED!");
    }
    return ok;
}
