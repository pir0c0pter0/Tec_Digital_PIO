/**
 * ============================================================================
 * GATT CONFIGURATION SERVICE - IMPLEMENTACAO
 * ============================================================================
 *
 * Callbacks de acesso GATT para as 4 caracteristicas de configuracao:
 * Volume, Brightness, Driver Name e Time Sync.
 *
 * Validacao de escritas usa gatt_validation.h (gatt_validate_write + gatt_read_write_data).
 * Eventos de configuracao sao postados na fila (config event queue) para
 * processamento seguro no Core 0 (system_task).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "services/ble/gatt/gatt_config.h"
#include "services/ble/gatt/gatt_validation.h"
#include "services/ble/gatt/gatt_server.h"
#include "config/app_config.h"
#include "config/ble_uuids.h"
#include "services/nvs/nvs_manager.h"

// NimBLE
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "os/os_mbuf.h"

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ESP-IDF
#include "esp_log.h"

#include <string.h>

// ============================================================================
// Extern "C" wrapper -- gatt_config.h declara funcoes com C linkage
// ============================================================================

extern "C" {

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "GATT_CONFIG";

// ============================================================================
// ESTADO DE NOTIFICACAO
// ============================================================================

static uint16_t s_conn_handle = 0;
static bool s_volume_notify_enabled = false;
static bool s_brightness_notify_enabled = false;

// ============================================================================
// FILA DE EVENTOS DE CONFIGURACAO
// ============================================================================

static QueueHandle_t s_config_queue = NULL;
static constexpr uint8_t CONFIG_EVENT_QUEUE_SIZE = 8;

// ============================================================================
// INICIALIZACAO DA FILA
// ============================================================================

bool config_event_queue_init(void) {
    if (s_config_queue != NULL) {
        ESP_LOGW(TAG, "Fila de eventos de config ja inicializada");
        return true;
    }

    s_config_queue = xQueueCreate(CONFIG_EVENT_QUEUE_SIZE, sizeof(ConfigEvent));
    if (s_config_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar fila de eventos de config");
        return false;
    }

    ESP_LOGI(TAG, "Fila de eventos de config criada (capacidade: %d)", CONFIG_EVENT_QUEUE_SIZE);
    return true;
}

// ============================================================================
// ENVIO DE EVENTOS (qualquer contexto -- non-blocking)
// ============================================================================

void config_post_event(ConfigEventType type, uint8_t value_u8) {
    if (s_config_queue == NULL) {
        ESP_LOGW(TAG, "Fila de config nao inicializada, evento descartado");
        return;
    }

    ConfigEvent evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = type;
    evt.value_u8 = value_u8;

    if (xQueueSend(s_config_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila de config cheia, evento descartado (type=%d)", static_cast<int>(type));
    }
}

void config_post_event_driver(uint8_t driver_id, const char* name) {
    if (s_config_queue == NULL) {
        ESP_LOGW(TAG, "Fila de config nao inicializada, evento descartado");
        return;
    }

    ConfigEvent evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = CONFIG_EVT_DRIVER_NAME;
    evt.driver_id = driver_id;

    if (name != NULL) {
        strncpy(evt.name, name, sizeof(evt.name) - 1);
        evt.name[sizeof(evt.name) - 1] = '\0';
    }

    if (xQueueSend(s_config_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila de config cheia, evento driver name descartado");
    }
}

void config_post_event_time(uint32_t timestamp) {
    if (s_config_queue == NULL) {
        ESP_LOGW(TAG, "Fila de config nao inicializada, evento descartado");
        return;
    }

    ConfigEvent evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = CONFIG_EVT_TIME_SYNC;
    evt.value_u32 = timestamp;

    if (xQueueSend(s_config_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila de config cheia, evento time sync descartado");
    }
}

// ============================================================================
// PROCESSAMENTO DE EVENTOS (system_task Core 0 -- LVGL-safe)
// ============================================================================

bool config_process_events(void (*handler)(const ConfigEvent& evt)) {
    if (s_config_queue == NULL || handler == NULL) {
        return false;
    }

    bool processed = false;
    ConfigEvent evt;

    while (xQueueReceive(s_config_queue, &evt, 0) == pdTRUE) {
        handler(evt);
        processed = true;
    }

    return processed;
}

// ============================================================================
// ACESSORES DE LEITURA (NVS -- mutex protected)
// ============================================================================

uint8_t config_get_current_volume(void) {
    return NvsManager::getInstance()->loadVolume(AUDIO_VOLUME_DEFAULT);
}

uint8_t config_get_current_brightness(void) {
    return NvsManager::getInstance()->loadBrightness(100);
}

// ============================================================================
// CALLBACKS DE ACESSO GATT
// ============================================================================

int config_volume_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t volume = config_get_current_volume();
        int rc = os_mbuf_append(ctxt->om, &volume, 1);
        if (rc != 0) {
            ESP_LOGE(TAG, "Falha ao anexar volume ao mbuf: %d", rc);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        ESP_LOGD(TAG, "Volume lido: %d", volume);
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        int err = gatt_validate_write(ctxt, 1, 1);
        if (err != 0) return err;

        uint8_t buf[1];
        int len = gatt_read_write_data(ctxt, buf, sizeof(buf));
        if (len < 0) return BLE_ATT_ERR_UNLIKELY;

        if (buf[0] > AUDIO_VOLUME_MAX) {
            ESP_LOGW(TAG, "Volume fora de range: %d (max=%d)", buf[0], AUDIO_VOLUME_MAX);
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }

        config_post_event(CONFIG_EVT_VOLUME, buf[0]);
        ESP_LOGI(TAG, "Volume escrito via BLE: %d", buf[0]);
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

int config_brightness_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t brightness = config_get_current_brightness();
        int rc = os_mbuf_append(ctxt->om, &brightness, 1);
        if (rc != 0) {
            ESP_LOGE(TAG, "Falha ao anexar brightness ao mbuf: %d", rc);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        ESP_LOGD(TAG, "Brightness lido: %d", brightness);
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        int err = gatt_validate_write(ctxt, 1, 1);
        if (err != 0) return err;

        uint8_t buf[1];
        int len = gatt_read_write_data(ctxt, buf, sizeof(buf));
        if (len < 0) return BLE_ATT_ERR_UNLIKELY;

        if (buf[0] > 100) {
            ESP_LOGW(TAG, "Brightness fora de range: %d (max=100)", buf[0]);
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }

        config_post_event(CONFIG_EVT_BRIGHTNESS, buf[0]);
        ESP_LOGI(TAG, "Brightness escrito via BLE: %d", buf[0]);
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

int config_driver_name_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // Retorna 3 motoristas * 33 bytes (1 byte id + 32 bytes nome) = 99 bytes
        uint8_t buf[MAX_MOTORISTAS * 33];
        memset(buf, 0, sizeof(buf));

        for (int i = 0; i < MAX_MOTORISTAS; i++) {
            size_t offset = static_cast<size_t>(i) * 33;
            buf[offset] = static_cast<uint8_t>(i + 1); // driver_id 1-3

            // Carrega nome do NVS (retorna vazio se nao encontrado)
            NvsManager::getInstance()->loadDriverName(
                static_cast<uint8_t>(i), reinterpret_cast<char*>(&buf[offset + 1]), 32);
        }

        int rc = os_mbuf_append(ctxt->om, buf, sizeof(buf));
        if (rc != 0) {
            ESP_LOGE(TAG, "Falha ao anexar driver names ao mbuf: %d", rc);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        ESP_LOGD(TAG, "Driver names lidos: %zu bytes", sizeof(buf));
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Minimo 2 bytes (id + pelo menos 1 char), maximo 33 bytes (id + 32 nome)
        int err = gatt_validate_write(ctxt, 2, 33);
        if (err != 0) return err;

        uint8_t buf[33];
        memset(buf, 0, sizeof(buf));
        int len = gatt_read_write_data(ctxt, buf, sizeof(buf));
        if (len < 0) return BLE_ATT_ERR_UNLIKELY;

        // BLE usa driver_id 1-3, internamente 0-2
        uint8_t driver_id = buf[0] - 1;
        if (driver_id >= MAX_MOTORISTAS) {
            ESP_LOGW(TAG, "Driver ID fora de range: %d (BLE id=%d)", driver_id, buf[0]);
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }

        // Extrai nome com null-termination forcada (Pitfall 6)
        char name_buf[33];
        memset(name_buf, 0, sizeof(name_buf));
        size_t name_len = static_cast<size_t>(len - 1);
        if (name_len > 32) name_len = 32;
        memcpy(name_buf, &buf[1], name_len);
        name_buf[name_len] = '\0';

        config_post_event_driver(driver_id, name_buf);
        ESP_LOGI(TAG, "Driver name escrito via BLE: id=%d, name='%s'", driver_id, name_buf);
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

int config_time_sync_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    // Write-only: rejeita leitura
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        int err = gatt_validate_write(ctxt, 4, 4);
        if (err != 0) return err;

        uint8_t buf[4];
        int len = gatt_read_write_data(ctxt, buf, sizeof(buf));
        if (len < 0) return BLE_ATT_ERR_UNLIKELY;

        // Extrai timestamp little-endian
        uint32_t timestamp = static_cast<uint32_t>(buf[0])
                           | (static_cast<uint32_t>(buf[1]) << 8)
                           | (static_cast<uint32_t>(buf[2]) << 16)
                           | (static_cast<uint32_t>(buf[3]) << 24);

        if (timestamp == 0) {
            ESP_LOGW(TAG, "Timestamp invalido: 0");
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }

        config_post_event_time(timestamp);
        ESP_LOGI(TAG, "Time sync escrito via BLE: %lu", (unsigned long)timestamp);
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// ============================================================================
// NOTIFICACOES BLE
// ============================================================================

void notify_config_volume(void) {
    if (s_conn_handle == 0 || !s_volume_notify_enabled) return;

    uint8_t volume = config_get_current_volume();
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&volume, 1);
    if (om) {
        int rc = ble_gatts_notify_custom(s_conn_handle,
                                         gatt_config_volume_val_handle, om);
        if (rc != 0) {
            ESP_LOGW(TAG, "Notify volume failed: %d", rc);
        } else {
            ESP_LOGD(TAG, "Volume notify enviado: %d", volume);
        }
    }
}

void notify_config_brightness(void) {
    if (s_conn_handle == 0 || !s_brightness_notify_enabled) return;

    uint8_t brightness = config_get_current_brightness();
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&brightness, 1);
    if (om) {
        int rc = ble_gatts_notify_custom(s_conn_handle,
                                         gatt_config_brightness_val_handle, om);
        if (rc != 0) {
            ESP_LOGW(TAG, "Notify brightness failed: %d", rc);
        } else {
            ESP_LOGD(TAG, "Brightness notify enviado: %d", brightness);
        }
    }
}

// ============================================================================
// GERENCIAMENTO DE SUBSCRICAO E CONEXAO
// ============================================================================

void gatt_config_set_conn_handle(uint16_t handle) {
    s_conn_handle = handle;
}

void gatt_config_update_subscription(uint16_t attr_handle, bool notify) {
    if (attr_handle == gatt_config_volume_val_handle) {
        s_volume_notify_enabled = notify;
        ESP_LOGI(TAG, "Volume notify %s", notify ? "habilitado" : "desabilitado");
    } else if (attr_handle == gatt_config_brightness_val_handle) {
        s_brightness_notify_enabled = notify;
        ESP_LOGI(TAG, "Brightness notify %s", notify ? "habilitado" : "desabilitado");
    }
}

void gatt_config_reset_subscriptions(void) {
    s_volume_notify_enabled = false;
    s_brightness_notify_enabled = false;
    ESP_LOGI(TAG, "Config subscricoes resetadas");
}

} // extern "C"
