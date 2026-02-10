/**
 * ============================================================================
 * SERVICO BLE - IMPLEMENTACAO
 * ============================================================================
 *
 * Inicializacao NimBLE, GAP advertising, LE Secure Connections,
 * persistencia de bonds via NVS.
 *
 * Baseado no exemplo bleprph do ESP-IDF.
 *
 * ============================================================================
 */

#include "services/ble/ble_service.h"
#include "services/ble/ble_event_queue.h"
#include "services/ble/gatt/gatt_server.h"
#include "services/ble/gatt/gatt_journey.h"
#include "config/app_config.h"
#include "config/ble_uuids.h"
#include "utils/debug_utils.h"

// NimBLE
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
// Bond store: NimBLE auto-configura via CONFIG_BT_NIMBLE_NVS_PERSIST=y

// ESP-IDF
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_log.h"

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "BLE_SVC";

// ============================================================================
// SINGLETON
// ============================================================================

BleService* BleService::instance_ = nullptr;

BleService* BleService::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new BleService();
    }
    return instance_;
}

BleService::BleService()
    : status_(BleStatus::DISCONNECTED)
    , connHandle_(0)
    , currentMtu_(23)
    , statusCallback_(nullptr)
    , mutex_(nullptr)
    , initialized_(false)
    , ownAddrType_(0) {
    memset(deviceName_, 0, sizeof(deviceName_));
}

// ============================================================================
// INIT
// ============================================================================

bool BleService::init() {
    if (initialized_) {
        ESP_LOGW(TAG, "BLE ja inicializado");
        return true;
    }

    // Cria mutex para acesso thread-safe ao status
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(TAG, "Falha ao criar mutex BLE");
        return false;
    }

    // ========================================================================
    // 1. Inicializa NVS default (CRITICO para persistencia de bonds NimBLE)
    //    NimBLE usa namespace "nimble_bond" na particao "nvs" (default)
    // ========================================================================
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS default cheio, apagando e reinicializando...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar NVS default: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "NVS default inicializado para bonds BLE");

    // ========================================================================
    // 2. Inicializa NimBLE port
    //    ESP-IDF 5.3: nimble_port_init() ja inicializa controller BT internamente
    // ========================================================================
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar NimBLE port: %s", esp_err_to_name(ret));
        return false;
    }

    // ========================================================================
    // 3. Configura callbacks do host
    // ========================================================================
    ble_hs_cfg.reset_cb = onBleReset;
    ble_hs_cfg.sync_cb = onBleSync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // ========================================================================
    // 4. Configura seguranca: LE Secure Connections (Just Works)
    // ========================================================================
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;           // LE Secure Connections
    ble_hs_cfg.sm_mitm = 0;         // Just Works (sem MITM)
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ESP_LOGI(TAG, "Seguranca configurada: LE Secure Connections (Just Works)");

    // ========================================================================
    // 5. Inicializa GATT server (GAP, GATT, DIS, Journey, Diagnostics)
    // ========================================================================
    {
        int gatt_rc = gatt_svr_init();
        if (gatt_rc != 0) {
            ESP_LOGE(TAG, "Falha ao inicializar GATT server: %d", gatt_rc);
            return false;
        }
    }

    // ========================================================================
    // 6. Constroi nome do dispositivo com ultimos 2 bytes do MAC BT
    // ========================================================================
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(deviceName_, sizeof(deviceName_), "%s-%02X%02X",
             BLE_DEVICE_NAME_PREFIX, mac[4], mac[5]);

    ret = ble_svc_gap_device_name_set(deviceName_);
    if (ret != 0) {
        ESP_LOGW(TAG, "Falha ao definir nome GAP: %d", ret);
    }
    ESP_LOGI(TAG, "Nome do dispositivo: %s", deviceName_);

    // Bond store inicializado automaticamente pelo NimBLE via NVS_PERSIST=y

    // ========================================================================
    // 7. Define MTU preferido
    // ========================================================================
    ret = ble_att_set_preferred_mtu(BLE_MTU_PREFERRED);
    if (ret != 0) {
        ESP_LOGW(TAG, "Falha ao definir MTU preferido: %d", ret);
    }
    ESP_LOGI(TAG, "MTU preferido: %d", BLE_MTU_PREFERRED);

    // ========================================================================
    // 8. Inicializa fila de eventos BLE (ponte NimBLE -> UI)
    // ========================================================================
    if (!ble_event_queue_init()) {
        ESP_LOGE(TAG, "Falha ao criar fila de eventos BLE (UI nao recebera updates)");
        // Continua: BLE funciona, apenas sem updates de UI
    }

    // ========================================================================
    // 9. Inicia task do host NimBLE
    // ========================================================================
    nimble_port_freertos_init(bleHostTask);

    initialized_ = true;
    ESP_LOGI(TAG, "BLE inicializado com sucesso");

    return true;
}

// ============================================================================
// CALLBACKS DO HOST NIMBLE
// ============================================================================

void BleService::onBleSync() {
    ESP_LOGI(TAG, "NimBLE host sincronizado");

    // Infere tipo de endereco automaticamente
    int rc = ble_hs_id_infer_auto(0, &getInstance()->ownAddrType_);
    if (rc != 0) {
        ESP_LOGE(TAG, "Falha ao inferir tipo de endereco: %d", rc);
        return;
    }

    // Loga endereco BLE
    uint8_t addr[6] = {0};
    ble_hs_id_copy_addr(getInstance()->ownAddrType_, addr, nullptr);
    ESP_LOGI(TAG, "Endereco BLE: %02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

    // Inicia advertising
    getInstance()->startAdvertisingInternal();
}

void BleService::onBleReset(int reason) {
    ESP_LOGE(TAG, "NimBLE host reset! Razao: %d", reason);
}

void BleService::bleHostTask(void* param) {
    ESP_LOGI(TAG, "NimBLE host task iniciada");
    nimble_port_run();
    // nimble_port_run() so retorna quando nimble_port_stop() e chamado
    nimble_port_freertos_deinit();
}

// ============================================================================
// ADVERTISING
// ============================================================================

void BleService::startAdvertising() {
    if (!initialized_) {
        ESP_LOGW(TAG, "BLE nao inicializado, nao pode anunciar");
        return;
    }
    startAdvertisingInternal();
}

void BleService::stopAdvertising() {
    if (!initialized_) return;

    int rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "Falha ao parar advertising: %d", rc);
    }
}

void BleService::startAdvertisingInternal() {
    // Para advertising anterior se ativo
    ble_gap_adv_stop();

    // ========================================================================
    // Dados de advertising (max 31 bytes)
    // Flags (3) + Nome completo (2+15=17) + TX Power (3) = 23 bytes
    // ========================================================================
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = reinterpret_cast<const uint8_t*>(deviceName_);
    fields.name_len = strlen(deviceName_);
    fields.name_is_complete = 1;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Falha ao definir campos de advertising: %d", rc);
        return;
    }

    // ========================================================================
    // Scan response (max 31 bytes) -- UUIDs de servico custom aqui
    // Para nao exceder 31 bytes no pacote de advertising principal
    // ========================================================================
    struct ble_hs_adv_fields rsp_fields = {};
    rsp_fields.uuids128 = const_cast<ble_uuid128_t*>(&BLE_UUID_JOURNEY_SVC);
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 0;  // Nao cabe todos no scan response

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Falha ao definir scan response: %d", rc);
        return;
    }

    // ========================================================================
    // Parametros de advertising
    // ========================================================================
    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;   // Conectavel
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;   // Descobrivel
    adv_params.itvl_min = BLE_ADV_INTERVAL_MIN;
    adv_params.itvl_max = BLE_ADV_INTERVAL_MAX;

    rc = ble_gap_adv_start(ownAddrType_, nullptr, BLE_HS_FOREVER,
                           &adv_params, gapEventHandler, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "Falha ao iniciar advertising: %d", rc);
        return;
    }

    updateStatus(BleStatus::ADVERTISING);
    ble_post_event(BleStatus::ADVERTISING);
    ESP_LOGI(TAG, "Advertising iniciado: %s", deviceName_);
}

// ============================================================================
// GAP EVENT HANDLER
// ============================================================================

int BleService::gapEventHandler(struct ble_gap_event* event, void* arg) {
    BleService* self = getInstance();

    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Conectado! handle=%d", event->connect.conn_handle);
            self->connHandle_ = event->connect.conn_handle;
            self->updateStatus(BleStatus::CONNECTED);
            ble_post_event(BleStatus::CONNECTED, event->connect.conn_handle);
            gatt_journey_set_conn_handle(event->connect.conn_handle);

            // Seguranca sera iniciada pelo central (celular) quando necessario
            // Nao chamamos ble_gap_security_initiate() para evitar conflito com nRF Connect
        } else {
            ESP_LOGW(TAG, "Falha na conexao: status=%d", event->connect.status);
            self->startAdvertisingInternal();
        }
        break;
    }

    case BLE_GAP_EVENT_DISCONNECT: {
        ESP_LOGI(TAG, "Desconectado! razao=%d", event->disconnect.reason);
        self->connHandle_ = 0;
        self->currentMtu_ = 23;
        self->updateStatus(BleStatus::DISCONNECTED);
        ble_post_event(BleStatus::DISCONNECTED);
        gatt_journey_set_conn_handle(0);
        gatt_journey_reset_subscriptions();

        // Reinicia advertising
        self->startAdvertisingInternal();
        break;
    }

    case BLE_GAP_EVENT_ENC_CHANGE: {
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "Criptografia ativada (LE Secure Connections)");
            self->updateStatus(BleStatus::SECURED);
            ble_post_event(BleStatus::SECURED, event->enc_change.conn_handle);
        } else {
            ESP_LOGW(TAG, "Falha na criptografia: status=%d", event->enc_change.status);
        }
        break;
    }

    case BLE_GAP_EVENT_MTU: {
        uint16_t mtu = event->mtu.value;
        ESP_LOGI(TAG, "MTU negociado: %d (channel_id=%d)", mtu, event->mtu.channel_id);
        self->currentMtu_ = mtu;
        ble_post_event(BleStatus::CONNECTED, event->mtu.conn_handle, mtu);

        // Maximiza tamanho de pacote link-layer para throughput
        ble_hs_hci_util_set_data_len(event->mtu.conn_handle, 251, 2120);
        break;
    }

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        // Deleta bond antigo e aceita novo pareamento
        ESP_LOGI(TAG, "Re-pareamento solicitado, deletando bond antigo...");
        struct ble_gap_conn_desc desc;
        int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    case BLE_GAP_EVENT_SUBSCRIBE: {
        ESP_LOGI(TAG, "Subscribe: conn=%d attr=%d cur_notify=%d cur_indicate=%d",
                 event->subscribe.conn_handle,
                 event->subscribe.attr_handle,
                 event->subscribe.cur_notify,
                 event->subscribe.cur_indicate);

        // Rastreia subscricao por caracteristica para notificacoes
        gatt_journey_update_subscription(event->subscribe.attr_handle,
                                         event->subscribe.cur_notify);
        break;
    }

    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        // Handler obrigatorio para completar pairing (mesmo Just Works)
        ESP_LOGI(TAG, "Passkey action: %d", event->passkey.params.action);
        struct ble_sm_io pkey = {};
        pkey.action = event->passkey.params.action;

        if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            // Comparacao numerica: aceita automaticamente (Just Works)
            ESP_LOGI(TAG, "Numeric comparison: %lu — aceitando", (unsigned long)event->passkey.params.numcmp);
            pkey.numcmp_accept = 1;
        }
        // Para IOACT_NONE: nenhuma acao necessaria, apenas injeta resposta vazia

        int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        if (rc != 0) {
            ESP_LOGW(TAG, "Falha ao injetar passkey IO: %d", rc);
        }
        break;
    }

    default:
        ESP_LOGD(TAG, "GAP event nao tratado: %d", event->type);
        break;
    }

    return 0;
}

// ============================================================================
// GETTERS
// ============================================================================

BleStatus BleService::getStatus() const {
    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        BleStatus s = status_;
        xSemaphoreGive(mutex_);
        return s;
    }
    return status_;
}

uint16_t BleService::getConnHandle() const {
    return connHandle_;
}

uint16_t BleService::getCurrentMtu() const {
    return currentMtu_;
}

void BleService::setStatusCallback(BleStatusCallback callback) {
    statusCallback_ = callback;
}

// ============================================================================
// INTERNALS
// ============================================================================

void BleService::updateStatus(BleStatus newStatus) {
    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        status_ = newStatus;
        xSemaphoreGive(mutex_);
    } else {
        status_ = newStatus;
    }

    if (statusCallback_ != nullptr) {
        statusCallback_(newStatus);
    }
}
