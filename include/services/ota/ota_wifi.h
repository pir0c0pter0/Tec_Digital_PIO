/**
 * ============================================================================
 * WI-FI STA MODULE PARA OTA - HEADER
 * ============================================================================
 *
 * Gerencia conexao Wi-Fi STA para transferencia OTA.
 * Todas as funcoes sao non-blocking para compatibilidade com LVGL.
 * A maquina de estados do OtaService faz polling via check_connected/check_failed.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef OTA_WIFI_H
#define OTA_WIFI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicia conexao Wi-Fi STA (non-blocking).
 * Configura Wi-Fi, registra handlers de eventos e inicia conexao.
 * Retorna imediatamente -- usar ota_wifi_check_connected/check_failed para polling.
 *
 * @param ssid SSID da rede Wi-Fi (max 32 chars)
 * @param password Senha da rede Wi-Fi (max 64 chars)
 * @return true se Wi-Fi foi iniciado com sucesso
 */
bool ota_wifi_connect(const char* ssid, const char* password);

/**
 * Verifica se conexao Wi-Fi foi estabelecida (non-blocking poll).
 * Retorna true quando IP foi obtido.
 *
 * @param out_ip_addr Ponteiro para receber endereco IP (network byte order)
 * @return true se conectado com IP valido
 */
bool ota_wifi_check_connected(uint32_t* out_ip_addr);

/**
 * Verifica se conexao Wi-Fi falhou definitivamente (non-blocking poll).
 * Retorna true quando todas as tentativas foram esgotadas.
 *
 * @return true se conexao falhou
 */
bool ota_wifi_check_failed(void);

/**
 * Desliga Wi-Fi completamente e libera recursos.
 * Desregistra handlers, para Wi-Fi, destroi netif.
 */
void ota_wifi_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_WIFI_H
