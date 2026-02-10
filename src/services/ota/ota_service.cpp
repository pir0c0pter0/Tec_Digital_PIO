/**
 * ============================================================================
 * SERVICO OTA - IMPLEMENTACAO
 * ============================================================================
 *
 * Maquina de estados OTA chamada periodicamente do system_task.
 * Orquestra Wi-Fi connect, BLE shutdown, HTTP server start,
 * firmware receive, SHA-256 verify e reboot.
 *
 * Todos os metodos sao non-blocking (nenhum wait com timeout longo)
 * para nao travar o LVGL no system_task.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "services/ota/ota_service.h"
#include "services/ota/ota_wifi.h"
#include "services/ota/ota_http_server.h"
#include "services/ble/ble_service.h"
#include "services/ble/gatt/gatt_ota_prov.h"
#include "config/app_config.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "OTA_SVC";

// ============================================================================
// SINGLETON
// ============================================================================

OtaService* OtaService::instance_ = nullptr;

OtaService* OtaService::getInstance() {
    if (!instance_) {
        instance_ = new OtaService();
    }
    return instance_;
}

// ============================================================================
// CONSTRUTOR
// ============================================================================

OtaService::OtaService()
    : state_(OTA_STATE_IDLE)
    , wifiCreds_{}
    , stateEnteredAt_(0)
    , ipAddr_(0)
    , progressCallback_(nullptr)
{
}

// ============================================================================
// INTERFACE PUBLICA
// ============================================================================

bool OtaService::startProvisioning(const OtaWifiCredentials& creds) {
    if (state_ != OTA_STATE_IDLE) {
        ESP_LOGW(TAG, "OTA nao pode iniciar: estado atual = %d", (int)state_);
        return false;
    }

    if (!creds.valid) {
        ESP_LOGE(TAG, "Credenciais Wi-Fi invalidas");
        return false;
    }

    // Copia credenciais
    memcpy(&wifiCreds_, &creds, sizeof(OtaWifiCredentials));

    ESP_LOGI(TAG, "Iniciando OTA com SSID: %s", wifiCreds_.ssid);

    // Inicia Wi-Fi (non-blocking)
    if (!ota_wifi_connect(wifiCreds_.ssid, wifiCreds_.password)) {
        ESP_LOGE(TAG, "Falha ao iniciar Wi-Fi");
        transitionTo(OTA_STATE_FAILED);
        return false;
    }

    transitionTo(OTA_STATE_CONNECTING_WIFI);
    return true;
}

void OtaService::abort() {
    if (state_ == OTA_STATE_IDLE || state_ == OTA_STATE_FAILED) {
        ESP_LOGW(TAG, "OTA nao pode ser cancelado: estado = %d", (int)state_);
        return;
    }

    ESP_LOGW(TAG, "OTA cancelado pelo usuario");
    transitionTo(OTA_STATE_ABORTING);
}

OtaState OtaService::getState() const {
    return state_;
}

void OtaService::setProgressCallback(void (*cb)(const OtaProgressEvent*)) {
    progressCallback_ = cb;
}

// ============================================================================
// PROCESS (chamado do system_task a cada 5ms)
// ============================================================================

void OtaService::process() {
    switch (state_) {
        case OTA_STATE_IDLE:
            processIdle();
            break;
        case OTA_STATE_CONNECTING_WIFI:
            processConnectingWifi();
            break;
        case OTA_STATE_WIFI_CONNECTED:
            processWifiConnected();
            break;
        case OTA_STATE_DISABLING_BLE:
            processDisablingBle();
            break;
        case OTA_STATE_STARTING_HTTP:
            processStartingHttp();
            break;
        case OTA_STATE_WAITING_FIRMWARE:
            processWaitingFirmware();
            break;
        case OTA_STATE_RECEIVING:
            processReceiving();
            break;
        case OTA_STATE_VERIFYING:
            processVerifying();
            break;
        case OTA_STATE_ABORTING:
            processAborting();
            break;
        case OTA_STATE_PROVISIONING:
        case OTA_STATE_REBOOTING:
        case OTA_STATE_FAILED:
            // Estados terminais ou transitorios -- nada a fazer
            break;
    }
}

// ============================================================================
// TRANSICAO DE ESTADO
// ============================================================================

void OtaService::transitionTo(OtaState newState) {
    ESP_LOGI(TAG, "Estado OTA: %d -> %d", (int)state_, (int)newState);
    state_ = newState;
    stateEnteredAt_ = (uint32_t)(esp_timer_get_time() / 1000);  // us -> ms

    // Notifica GATT OTA status (0 = sem erro por padrao)
    uint8_t error_code = (newState == OTA_STATE_FAILED) ? 1 : 0;
    ota_prov_set_state((uint8_t)newState, error_code);
}

// ============================================================================
// HANDLERS POR ESTADO
// ============================================================================

void OtaService::processIdle() {
    // Nada a fazer -- aguarda startProvisioning()
}

void OtaService::processConnectingWifi() {
    // Poll conexao Wi-Fi (non-blocking)
    if (ota_wifi_check_connected(&ipAddr_)) {
        ESP_LOGI(TAG, "Wi-Fi conectado, IP obtido");
        transitionTo(OTA_STATE_WIFI_CONNECTED);
        return;
    }

    if (ota_wifi_check_failed()) {
        ESP_LOGE(TAG, "Wi-Fi falhou apos todas as tentativas");
        transitionTo(OTA_STATE_FAILED);
        return;
    }

    // Timeout check
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if ((now - stateEnteredAt_) > OTA_WIFI_CONNECT_TIMEOUT_MS) {
        ESP_LOGE(TAG, "Timeout de conexao Wi-Fi (%d ms)", OTA_WIFI_CONNECT_TIMEOUT_MS);
        ota_wifi_shutdown();
        transitionTo(OTA_STATE_FAILED);
        return;
    }
}

void OtaService::processWifiConnected() {
    // Notifica IP via GATT (app precisa do IP antes de BLE desligar)
    ota_prov_set_ip_addr(ipAddr_);

    // Delay curto para garantir notificacao BLE antes de desligar
    // Nao bloqueante -- transiciona imediatamente na proxima chamada de process()
    transitionTo(OTA_STATE_DISABLING_BLE);
}

void OtaService::processDisablingBle() {
    // Desliga BLE (deve ser chamado de system_task -- OK)
    ESP_LOGI(TAG, "Desligando BLE para liberar RAM...");
    BleService::getInstance()->shutdown();

    transitionTo(OTA_STATE_STARTING_HTTP);
}

void OtaService::processStartingHttp() {
    // Inicializa fila de progresso
    if (!ota_progress_queue_init()) {
        ESP_LOGE(TAG, "Falha ao criar fila de progresso OTA");
        transitionTo(OTA_STATE_FAILED);
        return;
    }

    // Inicia servidor HTTP
    if (!ota_http_server_start()) {
        ESP_LOGE(TAG, "Falha ao iniciar servidor HTTP");
        transitionTo(OTA_STATE_FAILED);
        return;
    }

    ESP_LOGI(TAG, "Servidor HTTP OTA iniciado, aguardando firmware...");
    transitionTo(OTA_STATE_WAITING_FIRMWARE);
}

void OtaService::processWaitingFirmware() {
    // Poll fila de progresso -- quando firmware comeca a chegar,
    // o handler HTTP posta evento com estado RECEIVING
    ota_progress_process([](const OtaProgressEvent* evt) {
        OtaService* self = OtaService::getInstance();
        if (evt->state == OTA_STATE_RECEIVING) {
            self->transitionTo(OTA_STATE_RECEIVING);
        }
        if (self->progressCallback_) {
            self->progressCallback_(evt);
        }
    });
}

void OtaService::processReceiving() {
    // Poll fila de progresso e forward para callback de UI
    ota_progress_process([](const OtaProgressEvent* evt) {
        OtaService* self = OtaService::getInstance();

        // Forward para callback de UI (OtaScreen)
        if (self->progressCallback_) {
            self->progressCallback_(evt);
        }

        // Detecta transicoes de estado
        if (evt->state == OTA_STATE_VERIFYING) {
            self->transitionTo(OTA_STATE_VERIFYING);
        } else if (evt->state == OTA_STATE_REBOOTING) {
            self->transitionTo(OTA_STATE_REBOOTING);
        } else if (evt->state == OTA_STATE_FAILED) {
            self->transitionTo(OTA_STATE_FAILED);
        }
    });
}

void OtaService::processVerifying() {
    // Poll fila de progresso -- espera REBOOTING ou FAILED
    ota_progress_process([](const OtaProgressEvent* evt) {
        OtaService* self = OtaService::getInstance();

        if (self->progressCallback_) {
            self->progressCallback_(evt);
        }

        if (evt->state == OTA_STATE_REBOOTING) {
            self->transitionTo(OTA_STATE_REBOOTING);
        } else if (evt->state == OTA_STATE_FAILED) {
            self->transitionTo(OTA_STATE_FAILED);
        }
    });
}

void OtaService::processAborting() {
    ESP_LOGI(TAG, "Cancelando OTA...");

    // Para servidor HTTP
    ota_http_server_stop();

    // Desliga Wi-Fi
    ota_wifi_shutdown();

    ESP_LOGI(TAG, "OTA cancelado. Estado: FAILED");
    transitionTo(OTA_STATE_FAILED);
}
