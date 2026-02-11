/**
 * ============================================================================
 * SERVICO BLE - HEADER
 * ============================================================================
 *
 * Implementacao concreta do servico BLE usando NimBLE.
 * Gerencia inicializacao do stack, GAP advertising, seguranca
 * (LE Secure Connections) e persistencia de bonds via NVS.
 *
 * Singleton: BleService::getInstance()
 *
 * ============================================================================
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "interfaces/i_ble.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

// Forward declarations (NimBLE types)
struct ble_gap_event;

// ============================================================================
// CLASSE BLE SERVICE
// ============================================================================

class BleService : public IBleService {
public:
    /**
     * Obtem instancia singleton
     * @return Ponteiro para instancia unica
     */
    static BleService* getInstance();

    // IBleService interface
    bool init() override;
    void startAdvertising() override;
    void stopAdvertising() override;
    BleStatus getStatus() const override;
    uint16_t getConnHandle() const override;
    uint16_t getCurrentMtu() const override;
    void setStatusCallback(BleStatusCallback callback) override;

    /**
     * Desliga completamente o stack NimBLE, liberando ~50KB de SRAM interna.
     * IRREVERSIVEL -- dispositivo reinicia apos OTA.
     * DEVE ser chamado de system_task, NUNCA do NimBLE host task.
     */
    void shutdown();

private:
    BleService();
    ~BleService() = default;

    // Nao permite copia
    BleService(const BleService&) = delete;
    BleService& operator=(const BleService&) = delete;

    // ========================================================================
    // CALLBACKS ESTATICOS DO NIMBLE
    // ========================================================================

    /**
     * Handler de eventos GAP (conexao, desconexao, seguranca, MTU, etc.)
     */
    static int gapEventHandler(struct ble_gap_event* event, void* arg);

    /**
     * Callback de sincronizacao do host NimBLE
     * Chamado quando o host esta pronto para operar
     */
    static void onBleSync();

    /**
     * Callback de reset do host NimBLE
     */
    static void onBleReset(int reason);

    /**
     * Task do host NimBLE (executada pelo FreeRTOS)
     */
    static void bleHostTask(void* param);

    /**
     * Callback do timer de seguranca (inicia pairing apos delay)
     */
    static void securityTimerCallback(TimerHandle_t timer);

    // ========================================================================
    // METODOS INTERNOS
    // ========================================================================

    /**
     * Configura e inicia o advertising GAP
     * Chamado internamente apos sync do host
     */
    void startAdvertisingInternal();

    /**
     * Atualiza status e notifica callback
     */
    void updateStatus(BleStatus newStatus);

    // ========================================================================
    // MEMBROS
    // ========================================================================

    static BleService* instance_;

    BleStatus status_;
    uint16_t connHandle_;
    uint16_t currentMtu_;
    BleStatusCallback statusCallback_;
    SemaphoreHandle_t mutex_;
    bool initialized_;
    uint8_t ownAddrType_;
    char deviceName_[32];
    TimerHandle_t securityTimer_;
};

#endif // BLE_SERVICE_H
