/**
 * ============================================================================
 * GATT DIAGNOSTICS SERVICE - HEADER
 * ============================================================================
 *
 * Caracteristica de diagnosticos do sistema para leitura via BLE.
 * Fornece dados de heap, PSRAM, uptime e conexoes em formato binario.
 *
 * Formato: 16 bytes (DiagnosticsData packed struct)
 *
 * ============================================================================
 */

#ifndef GATT_DIAGNOSTICS_H
#define GATT_DIAGNOSTICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration NimBLE
struct ble_gatt_access_ctxt;

// ============================================================================
// ESTRUTURA DE DADOS BLE (packed, little-endian)
// ============================================================================

/**
 * Dados de diagnostico do sistema (16 bytes)
 */
struct __attribute__((packed)) DiagnosticsData {
    uint32_t free_heap;         // Heap interno livre (bytes)
    uint32_t min_free_heap;     // Menor heap livre desde boot (bytes)
    uint32_t psram_free;        // PSRAM livre (bytes)
    uint32_t uptime_seconds;    // Tempo desde boot (segundos)
};

// ============================================================================
// CALLBACK DE ACESSO GATT
// ============================================================================

/**
 * Callback de acesso para a caracteristica System Diagnostics.
 * Trata operacoes de leitura retornando dados de diagnostico.
 */
int diagnostics_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt* ctxt, void* arg);

#ifdef __cplusplus
}
#endif

#endif // GATT_DIAGNOSTICS_H
