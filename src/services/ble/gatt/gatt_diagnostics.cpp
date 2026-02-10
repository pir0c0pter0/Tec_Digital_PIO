/**
 * ============================================================================
 * GATT DIAGNOSTICS SERVICE - IMPLEMENTACAO
 * ============================================================================
 *
 * Callback de acesso para a caracteristica System Diagnostics.
 * Retorna dados de heap, PSRAM, uptime do sistema.
 *
 * Formato: 16 bytes (DiagnosticsData packed struct)
 *
 * ============================================================================
 */

#include "services/ble/gatt/gatt_diagnostics.h"

// NimBLE
#include "host/ble_hs.h"
#include "os/os_mbuf.h"

// ESP-IDF
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include <string.h>

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "GATT_DIAG";

// ============================================================================
// CALLBACK DE ACESSO GATT
// ============================================================================

int diagnostics_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        DiagnosticsData data;
        memset(&data, 0, sizeof(data));

        // Heap interno livre
        data.free_heap = static_cast<uint32_t>(
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

        // Menor heap livre desde boot
        data.min_free_heap = static_cast<uint32_t>(
            esp_get_minimum_free_heap_size());

        // PSRAM livre
        data.psram_free = static_cast<uint32_t>(
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

        // Uptime em segundos
        data.uptime_seconds = static_cast<uint32_t>(
            esp_timer_get_time() / 1000000);

        int rc = os_mbuf_append(ctxt->om, &data, sizeof(data));
        if (rc != 0) {
            ESP_LOGE(TAG, "Falha ao anexar diagnostics ao mbuf: %d", rc);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        ESP_LOGD(TAG, "Diagnostics lido: heap=%lu min=%lu psram=%lu uptime=%lus",
                 (unsigned long)data.free_heap,
                 (unsigned long)data.min_free_heap,
                 (unsigned long)data.psram_free,
                 (unsigned long)data.uptime_seconds);
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}
