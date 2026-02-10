/**
 * ============================================================================
 * FILA DE EVENTOS BLE - IMPLEMENTACAO
 * ============================================================================
 *
 * FreeRTOS queue para ponte BLE-to-UI.
 * Capacidade: 8 eventos. Non-blocking em ambos os lados.
 *
 * ============================================================================
 */

#include "services/ble/ble_event_queue.h"
#include "esp_log.h"

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "BLE_EVT_Q";

// ============================================================================
// FILA ESTATICA
// ============================================================================

static QueueHandle_t ble_event_queue = nullptr;

static constexpr uint8_t BLE_EVENT_QUEUE_SIZE = 8;

// ============================================================================
// INICIALIZACAO
// ============================================================================

bool ble_event_queue_init() {
    if (ble_event_queue != nullptr) {
        ESP_LOGW(TAG, "Fila de eventos BLE ja inicializada");
        return true;
    }

    ble_event_queue = xQueueCreate(BLE_EVENT_QUEUE_SIZE, sizeof(BleEvent));
    if (ble_event_queue == nullptr) {
        ESP_LOGE(TAG, "Falha ao criar fila de eventos BLE");
        return false;
    }

    ESP_LOGI(TAG, "Fila de eventos BLE criada (capacidade: %d)", BLE_EVENT_QUEUE_SIZE);
    return true;
}

// ============================================================================
// ENVIO DE EVENTO (qualquer contexto -- non-blocking)
// ============================================================================

void ble_post_event(BleStatus status, uint16_t conn_handle, uint16_t mtu) {
    if (ble_event_queue == nullptr) {
        ESP_LOGW(TAG, "Fila de eventos BLE nao inicializada, evento descartado");
        return;
    }

    BleEvent evt = {
        .status = status,
        .conn_handle = conn_handle,
        .mtu = mtu
    };

    if (xQueueSend(ble_event_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila de eventos BLE cheia, evento descartado (status=%d)",
                 static_cast<int>(status));
    }
}

// ============================================================================
// PROCESSAMENTO DE EVENTOS (system_task Core 0 -- LVGL-safe)
// ============================================================================

bool ble_process_events(void (*handler)(const BleEvent& evt)) {
    if (ble_event_queue == nullptr || handler == nullptr) {
        return false;
    }

    bool processed = false;
    BleEvent evt;

    while (xQueueReceive(ble_event_queue, &evt, 0) == pdTRUE) {
        handler(evt);
        processed = true;
    }

    return processed;
}
