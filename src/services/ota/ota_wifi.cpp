/**
 * ============================================================================
 * WI-FI STA MODULE PARA OTA - IMPLEMENTACAO
 * ============================================================================
 *
 * Gerencia conexao Wi-Fi STA para transferencia OTA.
 * Padroes non-blocking: ota_wifi_connect() inicia Wi-Fi e retorna
 * imediatamente. A maquina de estados do OtaService faz polling
 * via ota_wifi_check_connected() e ota_wifi_check_failed() sem
 * bloquear o system_task (LVGL).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "services/ota/ota_wifi.h"
#include "config/app_config.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <string.h>

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "OTA_WIFI";

// ============================================================================
// BITS DO EVENT GROUP
// ============================================================================

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

// ============================================================================
// ESTADO ESTATICO
// ============================================================================

static EventGroupHandle_t s_wifi_event_group = nullptr;
static int s_retry_num = 0;
static esp_netif_t* s_sta_netif = nullptr;
static esp_event_handler_instance_t s_instance_any_id = nullptr;
static esp_event_handler_instance_t s_instance_got_ip = nullptr;
static uint32_t s_ip_addr = 0;

// ============================================================================
// HANDLER DE EVENTOS WI-FI
// ============================================================================

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA iniciado, conectando...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < OTA_WIFI_MAX_RETRY) {
            s_retry_num++;
            ESP_LOGW(TAG, "Desconectado, tentativa %d/%d...", s_retry_num, OTA_WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Falha ao conectar apos %d tentativas", OTA_WIFI_MAX_RETRY);
            if (s_wifi_event_group) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        s_ip_addr = event->ip_info.ip.addr;
        ESP_LOGI(TAG, "IP obtido: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

// ============================================================================
// FUNCOES PUBLICAS
// ============================================================================

bool ota_wifi_connect(const char* ssid, const char* password) {
    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID vazio ou nulo");
        return false;
    }

    ESP_LOGI(TAG, "Iniciando Wi-Fi STA para SSID: %s", ssid);

    // Reseta estado
    s_retry_num = 0;
    s_ip_addr = 0;

    // Cria event group
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Falha ao criar event group");
        return false;
    }

    // Inicializa TCP/IP stack e event loop
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Falha esp_netif_init: %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
        return false;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Falha esp_event_loop_create_default: %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
        return false;
    }

    // Cria interface STA
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        ESP_LOGE(TAG, "Falha ao criar netif STA");
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
        return false;
    }

    // Inicializa Wi-Fi com config padrao
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha esp_wifi_init: %s", esp_err_to_name(err));
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = nullptr;
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
        return false;
    }

    // Registra handlers de eventos
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         &wifi_event_handler, nullptr,
                                         &s_instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         &wifi_event_handler, nullptr,
                                         &s_instance_got_ip);

    // Configura credenciais Wi-Fi
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password && strlen(password) > 0) {
        strncpy((char*)wifi_config.sta.password, password,
                sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    // Inicia Wi-Fi (non-blocking -- retorna antes de conectar)
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha esp_wifi_start: %s", esp_err_to_name(err));
        ota_wifi_shutdown();
        return false;
    }

    ESP_LOGI(TAG, "Wi-Fi STA iniciado, aguardando conexao...");
    return true;
}

bool ota_wifi_check_connected(uint32_t* out_ip_addr) {
    if (!s_wifi_event_group) {
        return false;
    }

    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    if (bits & WIFI_CONNECTED_BIT) {
        if (out_ip_addr) {
            *out_ip_addr = s_ip_addr;
        }
        return true;
    }

    return false;
}

bool ota_wifi_check_failed(void) {
    if (!s_wifi_event_group) {
        return false;
    }

    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_FAIL_BIT) != 0;
}

void ota_wifi_shutdown(void) {
    ESP_LOGI(TAG, "Desligando Wi-Fi...");

    // Desregistra handlers de eventos
    if (s_instance_any_id) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               s_instance_any_id);
        s_instance_any_id = nullptr;
    }
    if (s_instance_got_ip) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               s_instance_got_ip);
        s_instance_got_ip = nullptr;
    }

    // Para e desinicializa Wi-Fi
    esp_wifi_stop();
    esp_wifi_deinit();

    // Destroi interface de rede
    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = nullptr;
    }

    // Deleta event loop default
    esp_event_loop_delete_default();

    // Deleta event group
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
    }

    s_retry_num = 0;
    s_ip_addr = 0;

    ESP_LOGI(TAG, "Wi-Fi desligado com sucesso");
}
