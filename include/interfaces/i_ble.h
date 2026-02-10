/**
 * ============================================================================
 * INTERFACE DE BLE
 * ============================================================================
 *
 * Abstrai o servico BLE (Bluetooth Low Energy) do dispositivo.
 * Gerencia advertising, conexao, seguranca e bonding.
 *
 * ============================================================================
 */

#ifndef I_BLE_H
#define I_BLE_H

#include <stdint.h>

#ifdef __cplusplus

// ============================================================================
// TIPOS E ENUMERACOES
// ============================================================================

/**
 * Status da conexao BLE
 */
enum class BleStatus : uint8_t {
    DISCONNECTED = 0,   // Sem conexao, sem advertising
    ADVERTISING,        // Advertising ativo, aguardando conexao
    CONNECTED,          // Conectado, sem criptografia
    SECURED             // Conectado com LE Secure Connections (criptografado)
};

/**
 * Callback para mudanca de status BLE
 */
typedef void (*BleStatusCallback)(BleStatus newStatus);

// ============================================================================
// INTERFACE ABSTRATA
// ============================================================================

/**
 * Interface abstrata para o servico BLE
 *
 * Define contrato para inicializacao do stack NimBLE, GAP advertising,
 * seguranca (LE Secure Connections) e persistencia de bonds.
 */
class IBleService {
public:
    virtual ~IBleService() = default;

    /**
     * Inicializa o stack BLE (NimBLE)
     * @return true se inicializado com sucesso
     */
    virtual bool init() = 0;

    /**
     * Inicia o advertising BLE
     */
    virtual void startAdvertising() = 0;

    /**
     * Para o advertising BLE
     */
    virtual void stopAdvertising() = 0;

    /**
     * Obtem o status atual da conexao BLE
     * @return Status atual
     */
    virtual BleStatus getStatus() const = 0;

    /**
     * Obtem o handle da conexao ativa
     * @return Connection handle (0 se desconectado)
     */
    virtual uint16_t getConnHandle() const = 0;

    /**
     * Obtem o MTU negociado da conexao ativa
     * @return MTU atual (23 se nao negociado)
     */
    virtual uint16_t getCurrentMtu() const = 0;

    /**
     * Registra callback para mudancas de status
     * @param callback Funcao a ser chamada quando status mudar
     */
    virtual void setStatusCallback(BleStatusCallback callback) = 0;
};

#endif // __cplusplus

#endif // I_BLE_H
