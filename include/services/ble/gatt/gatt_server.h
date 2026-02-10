/**
 * ============================================================================
 * GATT SERVER - HEADER
 * ============================================================================
 *
 * Registro central de todos os servicos GATT do Teclado de Jornada:
 * - Device Information Service (SIG 0x180A)
 * - Journey Service (custom UUID)
 * - Diagnostics Service (custom UUID)
 *
 * Chamado durante init do NimBLE em ble_service.cpp.
 *
 * ============================================================================
 */

#ifndef GATT_SERVER_H
#define GATT_SERVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa todos os servicos GATT (GAP, GATT, DIS, Journey, Diagnostics).
 * Deve ser chamado uma unica vez durante o init do NimBLE.
 *
 * @return 0 em sucesso, codigo de erro NimBLE em falha
 */
int gatt_svr_init(void);

/**
 * Value handle da caracteristica Journey State (para notificacoes)
 */
extern uint16_t gatt_journey_state_val_handle;

/**
 * Value handle da caracteristica Ignition Status (para notificacoes)
 */
extern uint16_t gatt_ignition_val_handle;

#ifdef __cplusplus
}
#endif

#endif // GATT_SERVER_H
