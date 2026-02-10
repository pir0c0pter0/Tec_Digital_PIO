/**
 * ============================================================================
 * FILA DE EVENTOS BLE - HEADER
 * ============================================================================
 *
 * Ponte thread-safe entre callbacks NimBLE (qualquer core/contexto)
 * e o system_task no Core 0 (onde LVGL pode ser chamado com seguranca).
 *
 * Uso:
 *   - ble_post_event(): Chamado de callbacks NimBLE (Core 0/1, ISR-safe)
 *   - ble_process_events(): Chamado do system_task loop (Core 0, LVGL-safe)
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef BLE_EVENT_QUEUE_H
#define BLE_EVENT_QUEUE_H

#include "interfaces/i_ble.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stdint.h>

// ============================================================================
// TIPOS DE EVENTO
// ============================================================================

/**
 * Evento BLE para comunicacao entre NimBLE e UI
 */
struct BleEvent {
    BleStatus status;          // Novo status BLE
    uint16_t conn_handle;      // Handle da conexao (0 se desconectado)
    uint16_t mtu;              // MTU atual (relevante para MTU_CHANGED)
};

// ============================================================================
// API PUBLICA
// ============================================================================

/**
 * Inicializa a fila de eventos BLE
 * @return true se a fila foi criada com sucesso
 */
bool ble_event_queue_init();

/**
 * Envia evento BLE para a fila (non-blocking)
 *
 * Seguro para chamar de qualquer core/contexto (NimBLE callbacks, ISR).
 * Se a fila estiver cheia, o evento e descartado com warning no log.
 *
 * @param status Novo status BLE
 * @param conn_handle Handle da conexao (0 se desconectado)
 * @param mtu MTU negociado (0 se nao aplicavel)
 */
void ble_post_event(BleStatus status, uint16_t conn_handle = 0, uint16_t mtu = 0);

/**
 * Processa todos os eventos pendentes na fila
 *
 * DEVE ser chamado apenas do system_task no Core 0 (LVGL-safe).
 * Consome todos os eventos da fila e chama o handler para cada um.
 *
 * @param handler Funcao chamada para cada evento recebido
 * @return true se pelo menos um evento foi processado
 */
bool ble_process_events(void (*handler)(const BleEvent& evt));

#endif // BLE_EVENT_QUEUE_H
