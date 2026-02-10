/**
 * ============================================================================
 * SELF-TEST POS-OTA - HEADER
 * ============================================================================
 *
 * Funcao de self-test executada no primeiro boot apos OTA.
 * Verifica subsistemas criticos (LVGL, NVS, BLE, audio) e marca
 * o firmware como valido ou reverte para o firmware anterior.
 *
 * Chamada cedo no system_task, antes da sequencia normal de init.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef OTA_SELF_TEST_H
#define OTA_SELF_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Executa self-test se firmware esta em estado PENDING_VERIFY (pos-OTA).
 *
 * Comportamento:
 * - Se firmware NAO esta PENDING_VERIFY: retorna imediatamente (boot normal)
 * - Se PENDING_VERIFY: executa diagnostico de LVGL, NVS, BLE, audio
 *   - Todos passam: marca firmware valido (esp_ota_mark_app_valid_cancel_rollback)
 *   - Qualquer falha: marca invalido e reinicia (rollback automatico)
 *
 * Usa task watchdog com timeout de OTA_SELF_TEST_TIMEOUT_S (60s) para
 * proteger contra travamento do self-test.
 *
 * DEVE ser chamada de system_task (Core 0) apos splash e antes de init normal.
 */
void ota_self_test(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_SELF_TEST_H
