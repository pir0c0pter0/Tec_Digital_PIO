/**
 * ============================================================================
 * SERVICO OTA - HEADER
 * ============================================================================
 *
 * Maquina de estados OTA que orquestra o fluxo completo:
 * IDLE -> CONNECTING_WIFI -> WIFI_CONNECTED -> DISABLING_BLE ->
 * STARTING_HTTP -> WAITING_FIRMWARE -> RECEIVING -> VERIFYING -> REBOOTING
 *
 * Singleton: OtaService::getInstance()
 * process() DEVE ser chamado do system_task (Core 0) a cada 5ms.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include "interfaces/i_ota.h"
#include "services/ota/ota_types.h"

// ============================================================================
// CLASSE OTA SERVICE
// ============================================================================

class OtaService : public IOtaService {
public:
    /**
     * Obtem instancia singleton
     * @return Ponteiro para instancia unica
     */
    static OtaService* getInstance();

    // IOtaService interface
    bool startProvisioning(const OtaWifiCredentials& creds) override;
    void abort() override;
    OtaState getState() const override;
    void process() override;

    /**
     * Define callback de progresso OTA (chamado quando eventos de progresso chegam).
     * @param cb Funcao callback que recebe OtaProgressEvent
     */
    void setProgressCallback(void (*cb)(const OtaProgressEvent*));

private:
    OtaService();
    ~OtaService() = default;

    // Nao permite copia
    OtaService(const OtaService&) = delete;
    OtaService& operator=(const OtaService&) = delete;

    // ========================================================================
    // TRANSICAO DE ESTADO
    // ========================================================================

    /**
     * Transiciona para novo estado, loga e notifica GATT status.
     */
    void transitionTo(OtaState newState);

    // ========================================================================
    // HANDLERS POR ESTADO
    // ========================================================================

    void processIdle();
    void processConnectingWifi();
    void processWifiConnected();
    void processDisablingBle();
    void processStartingHttp();
    void processWaitingFirmware();
    void processReceiving();
    void processVerifying();
    void processAborting();

    // ========================================================================
    // MEMBROS
    // ========================================================================

    static OtaService* instance_;

    OtaState state_;
    OtaWifiCredentials wifiCreds_;
    uint32_t stateEnteredAt_;   // millis quando entrou no estado atual
    uint32_t ipAddr_;           // IP adquirido (network byte order)

    void (*progressCallback_)(const OtaProgressEvent*);
};

#endif // OTA_SERVICE_H
