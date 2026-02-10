/**
 * ============================================================================
 * UUIDs BLE CENTRALIZADOS
 * ============================================================================
 *
 * Todos os UUIDs customizados do protocolo BLE do Teclado de Jornada.
 *
 * Formato base: 0000XXXX-4A47-0000-4763-7365-0000000Y
 *   XXXX = ID curto do servico/caracteristica
 *   Y    = grupo do servico (1=Journey, 3=Diagnostics)
 *
 * IMPORTANTE: BLE_UUID128_INIT recebe bytes em LITTLE-ENDIAN (invertido).
 *
 * ============================================================================
 */

#ifndef BLE_UUIDS_H
#define BLE_UUIDS_H

#include "host/ble_uuid.h"

// ============================================================================
// SERVICO DE JORNADA (grupo 1)
// UUID base: 0000XXXX-4A47-0000-4763-7365-00000001
// ============================================================================

/**
 * Journey Service UUID: 00000100-4A47-0000-4763-7365-00000001
 * Little-endian: 01 00 00 00 | 65 73 63 47 | 00 00 47 4A | 00 01 00 00
 */
static const ble_uuid128_t BLE_UUID_JOURNEY_SVC =
    BLE_UUID128_INIT(0x01, 0x00, 0x00, 0x00, 0x65, 0x73, 0x63, 0x47,
                     0x00, 0x00, 0x4A, 0x47, 0x00, 0x01, 0x00, 0x00);

/**
 * Journey State Characteristic UUID: 00000101-4A47-0000-4763-7365-00000001
 * Little-endian: 01 00 00 00 | 65 73 63 47 | 00 00 47 4A | 01 01 00 00
 */
static const ble_uuid128_t BLE_UUID_JOURNEY_STATE_CHR =
    BLE_UUID128_INIT(0x01, 0x00, 0x00, 0x00, 0x65, 0x73, 0x63, 0x47,
                     0x00, 0x00, 0x4A, 0x47, 0x01, 0x01, 0x00, 0x00);

/**
 * Ignition Status Characteristic UUID: 00000102-4A47-0000-4763-7365-00000001
 * Little-endian: 01 00 00 00 | 65 73 63 47 | 00 00 47 4A | 02 01 00 00
 */
static const ble_uuid128_t BLE_UUID_IGNITION_STATUS_CHR =
    BLE_UUID128_INIT(0x01, 0x00, 0x00, 0x00, 0x65, 0x73, 0x63, 0x47,
                     0x00, 0x00, 0x4A, 0x47, 0x02, 0x01, 0x00, 0x00);

// ============================================================================
// SERVICO DE DIAGNOSTICOS (grupo 3)
// UUID base: 0000XXXX-4A47-0000-4763-7365-00000003
// ============================================================================

/**
 * Diagnostics Service UUID: 00000300-4A47-0000-4763-7365-00000003
 * Little-endian: 03 00 00 00 | 65 73 63 47 | 00 00 47 4A | 00 03 00 00
 */
static const ble_uuid128_t BLE_UUID_DIAGNOSTICS_SVC =
    BLE_UUID128_INIT(0x03, 0x00, 0x00, 0x00, 0x65, 0x73, 0x63, 0x47,
                     0x00, 0x00, 0x4A, 0x47, 0x00, 0x03, 0x00, 0x00);

/**
 * System Diagnostics Characteristic UUID: 00000301-4A47-0000-4763-7365-00000003
 * Little-endian: 03 00 00 00 | 65 73 63 47 | 00 00 47 4A | 01 03 00 00
 */
static const ble_uuid128_t BLE_UUID_SYSTEM_DIAGNOSTICS_CHR =
    BLE_UUID128_INIT(0x03, 0x00, 0x00, 0x00, 0x65, 0x73, 0x63, 0x47,
                     0x00, 0x00, 0x4A, 0x47, 0x01, 0x03, 0x00, 0x00);

#endif // BLE_UUIDS_H
