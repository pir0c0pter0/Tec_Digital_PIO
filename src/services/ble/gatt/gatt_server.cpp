/**
 * ============================================================================
 * GATT SERVER - IMPLEMENTACAO
 * ============================================================================
 *
 * Registro central de todos os servicos GATT:
 * - Device Information Service (SIG 0x180A) via ble_svc_dis
 * - Journey Service (custom UUID) com Journey State + Ignition Status
 * - Diagnostics Service (custom UUID) com System Diagnostics
 *
 * Chamado por BleService::init() durante inicializacao do NimBLE.
 *
 * ============================================================================
 */

#include "services/ble/gatt/gatt_server.h"
#include "services/ble/gatt/gatt_journey.h"
#include "services/ble/gatt/gatt_diagnostics.h"
#include "services/ble/gatt/gatt_config.h"
#include "config/ble_uuids.h"
#include "config/app_config.h"

// NimBLE
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// DIS header nao tem extern "C" guards -- wrapper necessario
extern "C" {
#include "services/dis/ble_svc_dis.h"
}

// ESP-IDF
#include "esp_log.h"

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "GATT_SVR";

// ============================================================================
// VALUE HANDLES (para notificacoes futuras)
// ============================================================================

uint16_t gatt_journey_state_val_handle = 0;
uint16_t gatt_ignition_val_handle = 0;
uint16_t gatt_config_volume_val_handle = 0;
uint16_t gatt_config_brightness_val_handle = 0;

// ============================================================================
// TABELA DE SERVICOS GATT
// ============================================================================

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    // ========================================================================
    // Journey Service (custom UUID)
    // ========================================================================
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &BLE_UUID_JOURNEY_SVC.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            // Journey State: leitura + notificacao
            {
                .uuid = &BLE_UUID_JOURNEY_STATE_CHR.u,
                .access_cb = journey_state_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .min_key_size = 0,
                .val_handle = &gatt_journey_state_val_handle,
            },
            // Ignition Status: leitura + notificacao
            {
                .uuid = &BLE_UUID_IGNITION_STATUS_CHR.u,
                .access_cb = ignition_status_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .min_key_size = 0,
                .val_handle = &gatt_ignition_val_handle,
            },
            { 0 }, // Terminador de caracteristicas
        },
    },
    // ========================================================================
    // Diagnostics Service (custom UUID)
    // ========================================================================
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &BLE_UUID_DIAGNOSTICS_SVC.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            // System Diagnostics: leitura apenas (diagnosticos sao polled)
            {
                .uuid = &BLE_UUID_SYSTEM_DIAGNOSTICS_CHR.u,
                .access_cb = diagnostics_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ,
                .min_key_size = 0,
                .val_handle = NULL,
            },
            { 0 }, // Terminador de caracteristicas
        },
    },
    // ========================================================================
    // Configuration Service (custom UUID)
    // ========================================================================
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &BLE_UUID_CONFIG_SVC.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            // Volume: leitura + escrita + notificacao
            {
                .uuid = &BLE_UUID_CONFIG_VOLUME_CHR.u,
                .access_cb = config_volume_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .min_key_size = 0,
                .val_handle = &gatt_config_volume_val_handle,
            },
            // Brightness: leitura + escrita + notificacao
            {
                .uuid = &BLE_UUID_CONFIG_BRIGHTNESS_CHR.u,
                .access_cb = config_brightness_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .min_key_size = 0,
                .val_handle = &gatt_config_brightness_val_handle,
            },
            // Driver Name: leitura + escrita
            {
                .uuid = &BLE_UUID_CONFIG_DRIVER_NAME_CHR.u,
                .access_cb = config_driver_name_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .min_key_size = 0,
                .val_handle = NULL,
            },
            // Time Sync: escrita apenas
            {
                .uuid = &BLE_UUID_CONFIG_TIME_SYNC_CHR.u,
                .access_cb = config_time_sync_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_WRITE,
                .min_key_size = 0,
                .val_handle = NULL,
            },
            { 0 }, // Terminador de caracteristicas
        },
    },
    // ========================================================================
    // Terminador de servicos
    // ========================================================================
    { 0 },
};

// ============================================================================
// INIT
// ============================================================================

int gatt_svr_init(void) {
    int rc;

    // 1. Inicializa servicos built-in GAP e GATT
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // 2. Inicializa Device Information Service (SIG 0x180A)
    ble_svc_dis_init();
    ble_svc_dis_manufacturer_name_set(APP_COMPANY);
    ble_svc_dis_model_number_set("GS-Jornada");
    ble_svc_dis_firmware_revision_set(APP_VERSION_STRING);
    ble_svc_dis_hardware_revision_set("ESP32-S3-R8");
    ble_svc_dis_software_revision_set(BLE_PROTOCOL_VERSION);

    ESP_LOGI(TAG, "DIS configurado: %s / GS-Jornada / %s / ESP32-S3-R8 / %s",
             APP_COMPANY, APP_VERSION_STRING, BLE_PROTOCOL_VERSION);

    // 3. Conta atributos GATT (custom services)
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Falha ao contar atributos GATT: %d", rc);
        return rc;
    }

    // 4. Registra servicos GATT
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Falha ao registrar servicos GATT: %d", rc);
        return rc;
    }

    ESP_LOGI(TAG, "GATT server inicializado: DIS + Journey (2 chr) + Diagnostics (1 chr) + Config (4 chr)");
    return 0;
}
