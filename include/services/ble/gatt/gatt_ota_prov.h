/**
 * ============================================================================
 * GATT OTA PROVISIONING SERVICE - HEADER
 * ============================================================================
 *
 * Servico de provisionamento OTA via BLE com 3 caracteristicas:
 * - Wi-Fi Credentials (write-only): SSID + senha para conexao OTA
 * - OTA Status (read+notify): estado atual da maquina OTA + codigo de erro
 * - IP Address (read+notify): endereco IP do dispositivo apos conexao Wi-Fi
 *
 * Fila de eventos de provisionamento (OTA prov event queue) para ponte
 * thread-safe entre callbacks GATT (NimBLE task) e system_task (Core 0).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef GATT_OTA_PROV_H
#define GATT_OTA_PROV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "services/ota/ota_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration NimBLE
struct ble_gatt_access_ctxt;

// ============================================================================
// CALLBACKS DE ACESSO GATT
// ============================================================================

/**
 * Callback de acesso para Wi-Fi Credentials (write-only)
 * Formato: [1B ssid_len][ssid bytes][1B pwd_len][pwd bytes]
 * Min 4 bytes, max 98 bytes
 */
int ota_prov_wifi_creds_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt* ctxt, void* arg);

/**
 * Callback de acesso para OTA Status (read-only)
 * Retorna 2 bytes: [state, error_code]
 */
int ota_prov_status_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt* ctxt, void* arg);

/**
 * Callback de acesso para IP Address (read-only)
 * Retorna 4 bytes com endereco IP (network byte order)
 */
int ota_prov_ip_addr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt* ctxt, void* arg);

// ============================================================================
// FILA DE EVENTOS DE PROVISIONAMENTO
// ============================================================================

/**
 * Inicializa a fila de eventos de provisionamento OTA (FreeRTOS queue)
 * @return true se criada com sucesso
 */
bool ota_prov_event_queue_init(void);

/**
 * Processa eventos pendentes na fila de provisionamento
 * DEVE ser chamado do system_task (Core 0)
 * @param handler Funcao chamada para cada evento
 * @return true se pelo menos um evento foi processado
 */
bool ota_prov_process_events(void (*handler)(const OtaProvEvent* evt));

// ============================================================================
// GERENCIAMENTO DE CONEXAO E SUBSCRICAO
// ============================================================================

/**
 * Define o connection handle para notificacoes OTA
 * @param handle Connection handle (0 para limpar)
 */
void ota_prov_set_conn_handle(uint16_t handle);

/**
 * Atualiza estado de subscricao por caracteristica
 * @param attr_handle Handle do atributo
 * @param notify true se cliente habilitou notificacoes
 */
void gatt_ota_prov_update_subscription(uint16_t attr_handle, bool notify);

/**
 * Reseta todas as subscricoes OTA (chamado em disconnect)
 */
void ota_prov_reset_subscriptions(void);

// ============================================================================
// ATUALIZACAO DE ESTADO (chamado pelo OtaService)
// ============================================================================

/**
 * Atualiza estado OTA cached e notifica cliente subscrito
 * @param state Estado OTA atual (OtaState cast para uint8_t)
 * @param error_code Codigo de erro (0 = sem erro)
 */
void ota_prov_set_state(uint8_t state, uint8_t error_code);

/**
 * Atualiza endereco IP cached e notifica cliente subscrito
 * @param ip_addr Endereco IP em network byte order
 */
void ota_prov_set_ip_addr(uint32_t ip_addr);

// ============================================================================
// VALUE HANDLES (para notificacoes -- definidos em gatt_server.cpp)
// ============================================================================

extern uint16_t gatt_ota_prov_status_val_handle;
extern uint16_t gatt_ota_prov_ip_val_handle;

#ifdef __cplusplus
}
#endif

#endif // GATT_OTA_PROV_H
