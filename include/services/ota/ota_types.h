/**
 * ============================================================================
 * TIPOS E DEFINICOES DE OTA
 * ============================================================================
 *
 * Tipos compartilhados pelo sistema de OTA:
 * - OtaState: estados da maquina de estados OTA
 * - OtaProgressEvent: progresso de download
 * - OtaWifiCredentials: credenciais Wi-Fi recebidas via BLE
 * - OtaProvEvent: evento da fila de provisionamento BLE -> system_task
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef OTA_TYPES_H
#define OTA_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ESTADOS DA MAQUINA DE ESTADOS OTA
// ============================================================================

/**
 * Estados possiveis do processo OTA.
 * Transicoes controladas pelo OtaService.
 */
typedef enum {
    OTA_STATE_IDLE = 0,         // Aguardando comando de inicio
    OTA_STATE_PROVISIONING,     // Credenciais Wi-Fi recebidas, preparando
    OTA_STATE_CONNECTING_WIFI,  // Conectando ao Wi-Fi
    OTA_STATE_WIFI_CONNECTED,   // Wi-Fi conectado, pronto para HTTP
    OTA_STATE_DISABLING_BLE,    // Desligando BLE para liberar RAM
    OTA_STATE_STARTING_HTTP,    // Iniciando servidor HTTP para receber firmware
    OTA_STATE_WAITING_FIRMWARE, // Servidor HTTP ativo, aguardando upload
    OTA_STATE_RECEIVING,        // Recebendo firmware via HTTP
    OTA_STATE_VERIFYING,        // Verificando integridade do firmware
    OTA_STATE_REBOOTING,        // Reiniciando para aplicar firmware
    OTA_STATE_ABORTING,         // Cancelando OTA em andamento
    OTA_STATE_FAILED,           // Falha no processo OTA
} OtaState;

// ============================================================================
// EVENTO DE PROGRESSO OTA
// ============================================================================

/**
 * Informacoes de progresso durante download de firmware.
 * Usado para atualizar UI e notificar app via BLE.
 */
typedef struct {
    uint8_t percent;            // Progresso 0-100%
    uint32_t bytes_received;    // Bytes recebidos ate agora
    uint32_t bytes_total;       // Tamanho total do firmware
    OtaState state;             // Estado atual da maquina
} OtaProgressEvent;

// ============================================================================
// CREDENCIAIS WI-FI
// ============================================================================

/**
 * Credenciais Wi-Fi recebidas via BLE para conexao OTA.
 * SSID max 32 bytes (IEEE 802.11), senha max 64 bytes (WPA2-PSK).
 */
typedef struct {
    char ssid[33];              // SSID null-terminated (max 32 chars + '\0')
    char password[65];          // Senha null-terminated (max 64 chars + '\0')
    bool valid;                 // true se credenciais foram parseadas com sucesso
} OtaWifiCredentials;

// ============================================================================
// EVENTOS DE PROVISIONAMENTO OTA (fila BLE -> system_task)
// ============================================================================

/**
 * Tipos de evento do servico de provisionamento OTA via BLE
 */
typedef enum {
    OTA_PROV_EVT_WIFI_CREDS = 0,   // Credenciais Wi-Fi recebidas
} OtaProvEventType;

/**
 * Evento de provisionamento OTA para a fila BLE -> Core 0
 */
typedef struct {
    OtaProvEventType type;          // Tipo do evento
    OtaWifiCredentials creds;       // Credenciais (valido apenas para OTA_PROV_EVT_WIFI_CREDS)
} OtaProvEvent;

#ifdef __cplusplus
}
#endif

#endif // OTA_TYPES_H
