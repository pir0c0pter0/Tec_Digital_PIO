/**
 * ============================================================================
 * GATT JOURNEY SERVICE - HEADER
 * ============================================================================
 *
 * Caracteristicas de jornada e ignicao para o servico BLE Journey.
 * Fornece dados binarios empacotados para leitura/notificacao via BLE.
 *
 * Formato Journey State: 3 motoristas * 8 bytes = 24 bytes
 * Formato Ignition: 8 bytes (status + duracao)
 *
 * ============================================================================
 */

#ifndef GATT_JOURNEY_H
#define GATT_JOURNEY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration NimBLE
struct ble_gatt_access_ctxt;

// ============================================================================
// ESTRUTURAS DE DADOS BLE (packed, little-endian)
// ============================================================================

/**
 * Dados de estado de jornada de um motorista (8 bytes)
 */
struct __attribute__((packed)) JourneyStateData {
    uint8_t  motorist_id;       // ID do motorista (1-3)
    uint8_t  state;             // EstadoJornada enum value
    uint8_t  active;            // 1 se ativo, 0 se inativo
    uint8_t  reserved;          // Reservado para alinhamento
    uint32_t time_in_state;     // Tempo no estado atual (ms)
};

/**
 * Dados de status da ignicao (8 bytes)
 */
struct __attribute__((packed)) IgnitionData {
    uint8_t  status;            // 1=ligada, 0=desligada
    uint8_t  reserved[3];       // Reservado para alinhamento
    uint32_t duration_ms;       // Duracao no estado atual (ms)
};

// ============================================================================
// FUNCOES DE EMPACOTAMENTO
// ============================================================================

/**
 * Empacota estados de jornada de todos os motoristas em buffer binario.
 *
 * @param buf Buffer de destino (minimo MAX_MOTORISTAS * sizeof(JourneyStateData))
 * @param buf_len Tamanho do buffer
 * @return Numero de bytes escritos, ou -1 em erro
 */
int pack_journey_states(uint8_t* buf, size_t buf_len);

/**
 * Empacota dados de ignicao em buffer binario.
 *
 * @param buf Buffer de destino (minimo sizeof(IgnitionData))
 * @param buf_len Tamanho do buffer
 * @return Numero de bytes escritos, ou -1 em erro
 */
int pack_ignition_data(uint8_t* buf, size_t buf_len);

// ============================================================================
// CALLBACKS DE ACESSO GATT
// ============================================================================

/**
 * Callback de acesso para a caracteristica Journey State.
 * Trata operacoes de leitura retornando dados empacotados.
 */
int journey_state_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg);

/**
 * Callback de acesso para a caracteristica Ignition Status.
 * Trata operacoes de leitura retornando dados empacotados.
 */
int ignition_status_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt* ctxt, void* arg);

#ifdef __cplusplus
}
#endif

#endif // GATT_JOURNEY_H
