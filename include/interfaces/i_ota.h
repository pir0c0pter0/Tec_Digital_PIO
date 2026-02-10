/**
 * ============================================================================
 * INTERFACE DE OTA
 * ============================================================================
 *
 * Abstrai o servico de OTA (Over-The-Air firmware update).
 * Gerencia o fluxo completo: provisioning Wi-Fi, download HTTP, rollback.
 *
 * ============================================================================
 */

#ifndef I_OTA_H
#define I_OTA_H

#include "services/ota/ota_types.h"

#ifdef __cplusplus

// ============================================================================
// INTERFACE ABSTRATA
// ============================================================================

/**
 * Interface abstrata para o servico de OTA
 *
 * Define contrato para o fluxo de atualizacao de firmware:
 * 1. App envia credenciais Wi-Fi via BLE
 * 2. Device conecta ao Wi-Fi
 * 3. Device desliga BLE (libera ~50KB RAM)
 * 4. Device inicia servidor HTTP
 * 5. App envia firmware via HTTP
 * 6. Device verifica e aplica firmware
 * 7. Device reinicia
 */
class IOtaService {
public:
    virtual ~IOtaService() = default;

    /**
     * Inicia o fluxo de provisionamento OTA com credenciais Wi-Fi
     * @param creds Credenciais Wi-Fi recebidas via BLE
     * @return true se o fluxo foi iniciado com sucesso
     */
    virtual bool startProvisioning(const OtaWifiCredentials& creds) = 0;

    /**
     * Cancela o OTA em andamento
     * Desconecta Wi-Fi, para servidor HTTP, retorna ao estado IDLE
     */
    virtual void abort() = 0;

    /**
     * Retorna o estado atual da maquina de estados OTA
     * @return Estado atual
     */
    virtual OtaState getState() const = 0;

    /**
     * Avanca a maquina de estados OTA
     * DEVE ser chamado periodicamente do system_task loop (Core 0)
     */
    virtual void process() = 0;
};

#endif // __cplusplus

#endif // I_OTA_H
