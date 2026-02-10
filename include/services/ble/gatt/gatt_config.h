/**
 * ============================================================================
 * GATT CONFIGURATION SERVICE - HEADER
 * ============================================================================
 *
 * Servico de configuracao BLE com 4 caracteristicas:
 * - Volume (read+write+notify): nivel de audio 0-21
 * - Brightness (read+write+notify): brilho do display 0-100%
 * - Driver Name (read+write): nome do motorista (1 byte id + até 32 bytes nome)
 * - Time Sync (write-only): sincronizacao de relogio (4 bytes unix timestamp)
 *
 * Fila de eventos de configuracao (config event queue) para ponte thread-safe
 * entre callbacks GATT (NimBLE task) e system_task (Core 0).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef GATT_CONFIG_H
#define GATT_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration NimBLE
struct ble_gatt_access_ctxt;

// ATT error code para valor fora de range (nao definido por NimBLE)
#ifndef BLE_ATT_ERR_VALUE_NOT_ALLOWED
#define BLE_ATT_ERR_VALUE_NOT_ALLOWED   0x13
#endif

// ============================================================================
// TIPOS DE EVENTO DE CONFIGURACAO
// ============================================================================

/**
 * Tipos de evento de configuracao enviados pela fila
 */
enum ConfigEventType : uint8_t {
    CONFIG_EVT_VOLUME = 0,
    CONFIG_EVT_BRIGHTNESS,
    CONFIG_EVT_DRIVER_NAME,
    CONFIG_EVT_TIME_SYNC,
};

/**
 * Evento de configuracao para a fila BLE -> Core 0
 */
struct ConfigEvent {
    ConfigEventType type;
    uint8_t driver_id;       // Para eventos de driver name (0-2)
    uint8_t value_u8;        // Para volume/brightness
    uint32_t value_u32;      // Para time sync (unix timestamp)
    char name[32];           // Para driver name (null-terminated)
};

// ============================================================================
// FILA DE EVENTOS DE CONFIGURACAO
// ============================================================================

/**
 * Inicializa a fila de eventos de configuracao (FreeRTOS queue, 8 items)
 * @return true se criada com sucesso
 */
bool config_event_queue_init(void);

/**
 * Envia evento de volume ou brightness para a fila (non-blocking)
 * @param type CONFIG_EVT_VOLUME ou CONFIG_EVT_BRIGHTNESS
 * @param value_u8 Valor do volume (0-21) ou brightness (0-100)
 */
void config_post_event(ConfigEventType type, uint8_t value_u8);

/**
 * Envia evento de driver name para a fila (non-blocking)
 * @param driver_id ID do motorista (0-2)
 * @param name Nome do motorista (null-terminated, max 32 bytes)
 */
void config_post_event_driver(uint8_t driver_id, const char* name);

/**
 * Envia evento de time sync para a fila (non-blocking)
 * @param timestamp Unix timestamp (segundos desde epoch)
 */
void config_post_event_time(uint32_t timestamp);

/**
 * Processa eventos pendentes na fila de configuracao
 * DEVE ser chamado do system_task (Core 0, LVGL-safe)
 * @param handler Funcao chamada para cada evento
 * @return true se pelo menos um evento foi processado
 */
bool config_process_events(void (*handler)(const ConfigEvent& evt));

// ============================================================================
// CALLBACKS DE ACESSO GATT
// ============================================================================

/**
 * Callback de acesso para Volume (read+write)
 * Read: retorna 1 byte com volume atual (NVS)
 * Write: valida 0-21, posta CONFIG_EVT_VOLUME
 */
int config_volume_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg);

/**
 * Callback de acesso para Brightness (read+write)
 * Read: retorna 1 byte com brilho atual (NVS)
 * Write: valida 0-100, posta CONFIG_EVT_BRIGHTNESS
 */
int config_brightness_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt* ctxt, void* arg);

/**
 * Callback de acesso para Driver Name (read+write)
 * Read: retorna 3 motoristas * 33 bytes (id + nome) = 99 bytes
 * Write: 1 byte driver_id (1-3) + até 32 bytes nome
 */
int config_driver_name_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt* ctxt, void* arg);

/**
 * Callback de acesso para Time Sync (write-only)
 * Write: 4 bytes little-endian unix timestamp
 */
int config_time_sync_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt* ctxt, void* arg);

// ============================================================================
// NOTIFICACOES BLE
// ============================================================================

/**
 * Envia notificacao com volume atual para cliente subscrito
 */
void notify_config_volume(void);

/**
 * Envia notificacao com brightness atual para cliente subscrito
 */
void notify_config_brightness(void);

// ============================================================================
// GERENCIAMENTO DE SUBSCRICAO E CONEXAO
// ============================================================================

/**
 * Define o connection handle para notificacoes de config
 * @param handle Connection handle (0 para limpar)
 */
void gatt_config_set_conn_handle(uint16_t handle);

/**
 * Atualiza estado de subscricao por caracteristica
 * @param attr_handle Handle do atributo
 * @param notify true se cliente habilitou notificacoes
 */
void gatt_config_update_subscription(uint16_t attr_handle, bool notify);

/**
 * Reseta todas as subscricoes de config (chamado em disconnect)
 */
void gatt_config_reset_subscriptions(void);

// ============================================================================
// ACESSORES DE LEITURA (valor atual cached do NVS)
// ============================================================================

/**
 * Retorna volume atual lido do NVS
 * @return Volume 0-21
 */
uint8_t config_get_current_volume(void);

/**
 * Retorna brightness atual lido do NVS
 * @return Brightness 0-100
 */
uint8_t config_get_current_brightness(void);

#ifdef __cplusplus
}
#endif

#endif // GATT_CONFIG_H
