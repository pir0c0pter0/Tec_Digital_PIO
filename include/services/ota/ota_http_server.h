/**
 * ============================================================================
 * SERVIDOR HTTP PARA OTA - HEADER
 * ============================================================================
 *
 * Servidor HTTP para receber firmware via POST /update.
 * Recebe firmware em chunks de 4KB (heap), escreve na particao OTA inativa,
 * computa SHA-256 incremental e verifica contra header X-SHA256.
 *
 * Fila de progresso OTA para ponte HTTP task -> system_task (UI updates).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef OTA_HTTP_SERVER_H
#define OTA_HTTP_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "services/ota/ota_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SERVIDOR HTTP
// ============================================================================

/**
 * Inicia servidor HTTP na porta OTA_HTTP_SERVER_PORT (8080).
 * Registra endpoints /update (POST) e /status (GET).
 *
 * @return true se servidor iniciou com sucesso
 */
bool ota_http_server_start(void);

/**
 * Para o servidor HTTP e libera recursos.
 */
void ota_http_server_stop(void);

// ============================================================================
// FILA DE PROGRESSO OTA
// ============================================================================

/**
 * Inicializa a fila de progresso OTA (FreeRTOS queue, tamanho 1).
 * Usa xQueueOverwrite para sempre manter o evento mais recente.
 *
 * @return true se fila criada com sucesso
 */
bool ota_progress_queue_init(void);

/**
 * Publica evento de progresso na fila (non-blocking, sobrescreve anterior).
 * Chamado do handler HTTP (contexto diferente do system_task).
 *
 * @param percent Progresso 0-100%
 * @param received Bytes recebidos ate agora
 * @param total Total de bytes esperados
 * @param state Estado atual do OTA
 */
void ota_progress_post(uint8_t percent, uint32_t received, uint32_t total, OtaState state);

/**
 * Processa evento de progresso pendente na fila (non-blocking).
 * DEVE ser chamado do system_task (Core 0).
 *
 * @param handler Funcao callback para processar evento
 * @return true se um evento foi processado
 */
bool ota_progress_process(void (*handler)(const OtaProgressEvent* evt));

#ifdef __cplusplus
}
#endif

#endif // OTA_HTTP_SERVER_H
