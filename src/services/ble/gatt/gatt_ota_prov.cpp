/**
 * ============================================================================
 * GATT OTA PROVISIONING SERVICE - IMPLEMENTACAO
 * ============================================================================
 *
 * Callbacks de acesso GATT para as 3 caracteristicas de provisionamento OTA:
 * Wi-Fi Credentials (write), OTA Status (read+notify), IP Address (read+notify).
 *
 * Validacao de escritas usa gatt_validation.h.
 * Eventos de provisionamento sao postados na fila (OTA prov event queue) para
 * processamento seguro no Core 0 (system_task).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "services/ble/gatt/gatt_ota_prov.h"
#include "services/ble/gatt/gatt_validation.h"
#include "services/ble/gatt/gatt_server.h"
#include "config/app_config.h"
#include "config/ble_uuids.h"

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
// Extern "C" wrapper -- gatt_ota_prov.h declara funcoes com C linkage
// ============================================================================

extern "C" {

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "GATT_OTA_PROV";

// ============================================================================
// ESTADO INTERNO
// ============================================================================

static uint16_t s_ota_prov_conn_handle = 0;
static bool s_ota_status_subscribed = false;
static bool s_ota_ip_subscribed = false;
static uint8_t s_current_ota_state = 0;    // OTA_STATE_IDLE
static uint8_t s_current_error_code = 0;
static uint32_t s_current_ip_addr = 0;

// ============================================================================
// FILA DE EVENTOS DE PROVISIONAMENTO
// ============================================================================

static QueueHandle_t s_ota_prov_event_queue = NULL;

// ============================================================================
// INICIALIZACAO DA FILA
// ============================================================================

bool ota_prov_event_queue_init(void) {
    if (s_ota_prov_event_queue != NULL) {
        ESP_LOGW(TAG, "Fila de eventos OTA prov ja inicializada");
        return true;
    }

    s_ota_prov_event_queue = xQueueCreate(OTA_PROV_EVENT_QUEUE_SIZE, sizeof(OtaProvEvent));
    if (s_ota_prov_event_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar fila de eventos OTA prov");
        return false;
    }

    ESP_LOGI(TAG, "Fila de eventos OTA prov criada (capacidade: %d)", OTA_PROV_EVENT_QUEUE_SIZE);
    return true;
}

// ============================================================================
// PROCESSAMENTO DE EVENTOS (system_task Core 0)
// ============================================================================

bool ota_prov_process_events(void (*handler)(const OtaProvEvent* evt)) {
    if (s_ota_prov_event_queue == NULL || handler == NULL) {
        return false;
    }

    bool processed = false;
    OtaProvEvent evt;

    while (xQueueReceive(s_ota_prov_event_queue, &evt, 0) == pdTRUE) {
        handler(&evt);
        processed = true;
    }

    return processed;
}

// ============================================================================
// CALLBACKS DE ACESSO GATT
// ============================================================================

int ota_prov_wifi_creds_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    // Write-only: rejeita leitura
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Formato: [1B ssid_len][ssid bytes][1B pwd_len][pwd bytes]
        // Min: 1 + 1 + 1 + 0 = 3 bytes (SSID min 1 char, pwd pode ser vazio)
        // Max: 1 + 32 + 1 + 64 = 98 bytes
        int err = gatt_validate_write(ctxt, 3, 98);
        if (err != 0) return err;

        uint8_t buf[98];
        memset(buf, 0, sizeof(buf));
        int len = gatt_read_write_data(ctxt, buf, sizeof(buf));
        if (len < 0) return BLE_ATT_ERR_UNLIKELY;

        // Parse ssid_len
        uint8_t ssid_len = buf[0];
        if (ssid_len == 0 || ssid_len > 32) {
            ESP_LOGW(TAG, "SSID len invalido: %d", ssid_len);
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }

        // Verifica se ha bytes suficientes para SSID + pwd_len
        if (len < (int)(1 + ssid_len + 1)) {
            ESP_LOGW(TAG, "Dados insuficientes: len=%d, esperado>=%d", len, 1 + ssid_len + 1);
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }

        // Parse pwd_len
        uint8_t pwd_len = buf[1 + ssid_len];
        if (pwd_len > 64) {
            ESP_LOGW(TAG, "Password len invalido: %d", pwd_len);
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }

        // Verifica se ha bytes suficientes para tudo
        if (len < (int)(1 + ssid_len + 1 + pwd_len)) {
            ESP_LOGW(TAG, "Dados insuficientes para pwd: len=%d, esperado=%d",
                     len, 1 + ssid_len + 1 + pwd_len);
            return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        }

        // Constroi credenciais
        OtaWifiCredentials creds;
        memset(&creds, 0, sizeof(creds));
        memcpy(creds.ssid, &buf[1], ssid_len);
        creds.ssid[ssid_len] = '\0';
        memcpy(creds.password, &buf[1 + ssid_len + 1], pwd_len);
        creds.password[pwd_len] = '\0';
        creds.valid = true;

        // Posta evento na fila
        OtaProvEvent evt;
        memset(&evt, 0, sizeof(evt));
        evt.type = OTA_PROV_EVT_WIFI_CREDS;
        evt.creds = creds;

        if (s_ota_prov_event_queue != NULL) {
            if (xQueueSend(s_ota_prov_event_queue, &evt, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Fila OTA prov cheia, evento descartado");
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        } else {
            ESP_LOGW(TAG, "Fila OTA prov nao inicializada");
            return BLE_ATT_ERR_UNLIKELY;
        }

        ESP_LOGI(TAG, "Wi-Fi creds recebidas via BLE: SSID='%s' (pwd_len=%d)", creds.ssid, pwd_len);
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

int ota_prov_status_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // Retorna 2 bytes: [state, error_code]
        uint8_t status_buf[2] = { s_current_ota_state, s_current_error_code };
        int rc = os_mbuf_append(ctxt->om, status_buf, sizeof(status_buf));
        if (rc != 0) {
            ESP_LOGE(TAG, "Falha ao anexar OTA status ao mbuf: %d", rc);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        ESP_LOGD(TAG, "OTA status lido: state=%d, error=%d", s_current_ota_state, s_current_error_code);
        return 0;
    }

    // Status e read-only
    return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
}

int ota_prov_ip_addr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // Retorna 4 bytes com IP address (network byte order)
        int rc = os_mbuf_append(ctxt->om, &s_current_ip_addr, sizeof(s_current_ip_addr));
        if (rc != 0) {
            ESP_LOGE(TAG, "Falha ao anexar IP addr ao mbuf: %d", rc);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        ESP_LOGD(TAG, "IP addr lido: 0x%08lx", (unsigned long)s_current_ip_addr);
        return 0;
    }

    // IP e read-only
    return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
}

// ============================================================================
// ATUALIZACAO DE ESTADO (chamado pelo OtaService)
// ============================================================================

void ota_prov_set_state(uint8_t state, uint8_t error_code) {
    s_current_ota_state = state;
    s_current_error_code = error_code;

    // Notifica cliente subscrito
    if (s_ota_prov_conn_handle != 0 && s_ota_status_subscribed) {
        uint8_t status_buf[2] = { state, error_code };
        struct os_mbuf *om = ble_hs_mbuf_from_flat(status_buf, sizeof(status_buf));
        if (om) {
            int rc = ble_gatts_notify_custom(s_ota_prov_conn_handle,
                                              gatt_ota_prov_status_val_handle, om);
            if (rc != 0) {
                ESP_LOGW(TAG, "Notify OTA status failed: %d", rc);
            } else {
                ESP_LOGD(TAG, "OTA status notify enviado: state=%d, error=%d", state, error_code);
            }
        }
    }
}

void ota_prov_set_ip_addr(uint32_t ip_addr) {
    s_current_ip_addr = ip_addr;

    // Notifica cliente subscrito
    if (s_ota_prov_conn_handle != 0 && s_ota_ip_subscribed) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(&ip_addr, sizeof(ip_addr));
        if (om) {
            int rc = ble_gatts_notify_custom(s_ota_prov_conn_handle,
                                              gatt_ota_prov_ip_val_handle, om);
            if (rc != 0) {
                ESP_LOGW(TAG, "Notify IP addr failed: %d", rc);
            } else {
                ESP_LOGD(TAG, "IP addr notify enviado: 0x%08lx", (unsigned long)ip_addr);
            }
        }
    }
}

// ============================================================================
// GERENCIAMENTO DE CONEXAO E SUBSCRICAO
// ============================================================================

void ota_prov_set_conn_handle(uint16_t handle) {
    s_ota_prov_conn_handle = handle;
}

void gatt_ota_prov_update_subscription(uint16_t attr_handle, bool notify) {
    if (attr_handle == gatt_ota_prov_status_val_handle) {
        s_ota_status_subscribed = notify;
        ESP_LOGI(TAG, "OTA status notify %s", notify ? "habilitado" : "desabilitado");
    } else if (attr_handle == gatt_ota_prov_ip_val_handle) {
        s_ota_ip_subscribed = notify;
        ESP_LOGI(TAG, "OTA IP addr notify %s", notify ? "habilitado" : "desabilitado");
    }
}

void ota_prov_reset_subscriptions(void) {
    s_ota_status_subscribed = false;
    s_ota_ip_subscribed = false;
    ESP_LOGI(TAG, "OTA prov subscricoes resetadas");
}

} // extern "C"
