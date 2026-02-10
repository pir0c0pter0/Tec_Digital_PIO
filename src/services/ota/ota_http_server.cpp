/**
 * ============================================================================
 * SERVIDOR HTTP PARA OTA - IMPLEMENTACAO
 * ============================================================================
 *
 * Servidor HTTP para receber firmware via POST /update.
 * Usa chunks de 4KB alocados em heap (nao stack -- evita stack overflow
 * conforme pitfall #3 da pesquisa de OTA).
 *
 * SHA-256 incremental via mbedtls para verificacao de integridade.
 * Fila de progresso (tamanho 1, xQueueOverwrite) para ponte
 * HTTP task -> system_task (UI).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "services/ota/ota_http_server.h"
#include "config/app_config.h"

#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_app_format.h"
#include "mbedtls/sha256.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "OTA_HTTP";

// ============================================================================
// ESTADO ESTATICO
// ============================================================================

static httpd_handle_t s_server = nullptr;
static QueueHandle_t s_ota_progress_queue = nullptr;
static uint8_t s_expected_sha256[32] = {};
static bool s_sha256_provided = false;

// ============================================================================
// UTILITARIOS
// ============================================================================

/**
 * Decodifica string hexadecimal (64 chars) para array de bytes (32 bytes).
 * @return true se decodificacao bem-sucedida
 */
static bool hex_to_bytes(const char* hex, size_t hex_len, uint8_t* out, size_t out_len) {
    if (hex_len != out_len * 2) {
        return false;
    }

    for (size_t i = 0; i < out_len; i++) {
        char high = hex[i * 2];
        char low  = hex[i * 2 + 1];

        uint8_t val = 0;

        if (high >= '0' && high <= '9')      val = (uint8_t)((high - '0') << 4);
        else if (high >= 'a' && high <= 'f') val = (uint8_t)((high - 'a' + 10) << 4);
        else if (high >= 'A' && high <= 'F') val = (uint8_t)((high - 'A' + 10) << 4);
        else return false;

        if (low >= '0' && low <= '9')      val |= (uint8_t)(low - '0');
        else if (low >= 'a' && low <= 'f') val |= (uint8_t)(low - 'a' + 10);
        else if (low >= 'A' && low <= 'F') val |= (uint8_t)(low - 'A' + 10);
        else return false;

        out[i] = val;
    }
    return true;
}

// ============================================================================
// FILA DE PROGRESSO OTA
// ============================================================================

bool ota_progress_queue_init(void) {
    if (s_ota_progress_queue != nullptr) {
        ESP_LOGW(TAG, "Fila de progresso OTA ja inicializada");
        return true;
    }

    s_ota_progress_queue = xQueueCreate(OTA_PROGRESS_QUEUE_SIZE, sizeof(OtaProgressEvent));
    if (!s_ota_progress_queue) {
        ESP_LOGE(TAG, "Falha ao criar fila de progresso OTA");
        return false;
    }

    ESP_LOGI(TAG, "Fila de progresso OTA criada (tamanho: %d)", OTA_PROGRESS_QUEUE_SIZE);
    return true;
}

void ota_progress_post(uint8_t percent, uint32_t received, uint32_t total, OtaState state) {
    if (!s_ota_progress_queue) {
        return;
    }

    OtaProgressEvent evt = {
        .percent = percent,
        .bytes_received = received,
        .bytes_total = total,
        .state = state,
    };

    xQueueOverwrite(s_ota_progress_queue, &evt);
}

bool ota_progress_process(void (*handler)(const OtaProgressEvent* evt)) {
    if (!s_ota_progress_queue || !handler) {
        return false;
    }

    OtaProgressEvent evt;
    if (xQueueReceive(s_ota_progress_queue, &evt, 0) == pdTRUE) {
        handler(&evt);
        return true;
    }

    return false;
}

// ============================================================================
// HANDLER DE UPLOAD DE FIRMWARE (/update POST)
// ============================================================================

static esp_err_t ota_upload_handler(httpd_req_t* req) {
    size_t content_len = req->content_len;

    ESP_LOGI(TAG, "Recebendo firmware: %u bytes", (unsigned)content_len);

    // Valida tamanho do conteudo
    if (content_len == 0) {
        ESP_LOGE(TAG, "Content-Length vazio");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content-Length vazio");
        return ESP_FAIL;
    }

    if (content_len > OTA_MAX_IMAGE_SIZE) {
        ESP_LOGE(TAG, "Firmware muito grande: %u > %u", (unsigned)content_len, (unsigned)OTA_MAX_IMAGE_SIZE);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware excede tamanho maximo");
        return ESP_FAIL;
    }

    // Parse header X-SHA256 (opcional)
    s_sha256_provided = false;
    char sha256_header[65] = {};
    if (httpd_req_get_hdr_value_len(req, "X-SHA256") == 64) {
        httpd_req_get_hdr_value_str(req, "X-SHA256", sha256_header, sizeof(sha256_header));
        if (hex_to_bytes(sha256_header, 64, s_expected_sha256, 32)) {
            s_sha256_provided = true;
            ESP_LOGI(TAG, "SHA-256 esperado recebido via header");
        } else {
            ESP_LOGW(TAG, "Header X-SHA256 invalido, ignorando verificacao");
        }
    }

    // Obtem particao OTA inativa
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "Particao OTA nao encontrada");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Particao OTA nao encontrada");
        ota_progress_post(0, 0, content_len, OTA_STATE_FAILED);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Particao destino: %s (offset=0x%lx, size=%lu)",
             update_partition->label,
             (unsigned long)update_partition->address,
             (unsigned long)update_partition->size);

    // Inicia OTA
    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin falhou: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Falha ao iniciar OTA");
        ota_progress_post(0, 0, content_len, OTA_STATE_FAILED);
        return ESP_FAIL;
    }

    // Inicializa SHA-256
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);  // 0 = SHA-256 (nao SHA-224)

    // Aloca buffer em HEAP (nao stack -- evita stack overflow)
    char* buf = (char*)malloc(OTA_RECEIVE_BUFFER_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "Falha ao alocar buffer de %d bytes", OTA_RECEIVE_BUFFER_SIZE);
        esp_ota_abort(ota_handle);
        mbedtls_sha256_free(&sha_ctx);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Sem memoria");
        ota_progress_post(0, 0, content_len, OTA_STATE_FAILED);
        return ESP_FAIL;
    }

    // Loop de recepcao
    size_t remaining = content_len;
    size_t received = 0;

    while (remaining > 0) {
        size_t to_read = remaining < OTA_RECEIVE_BUFFER_SIZE
                         ? remaining : OTA_RECEIVE_BUFFER_SIZE;

        int recv_len = httpd_req_recv(req, buf, to_read);

        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Timeout de recepcao, continuando...");
            continue;
        }

        if (recv_len <= 0) {
            ESP_LOGE(TAG, "Erro de recepcao: %d", recv_len);
            free(buf);
            esp_ota_abort(ota_handle);
            mbedtls_sha256_free(&sha_ctx);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro de recepcao");
            ota_progress_post(0, received, content_len, OTA_STATE_FAILED);
            return ESP_FAIL;
        }

        // Escreve na particao OTA
        err = esp_ota_write(ota_handle, buf, (size_t)recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write falhou: %s", esp_err_to_name(err));
            free(buf);
            esp_ota_abort(ota_handle);
            mbedtls_sha256_free(&sha_ctx);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Falha ao escrever firmware");
            ota_progress_post(0, received, content_len, OTA_STATE_FAILED);
            return ESP_FAIL;
        }

        // Atualiza SHA-256
        mbedtls_sha256_update(&sha_ctx, (const unsigned char*)buf, (size_t)recv_len);

        remaining -= (size_t)recv_len;
        received += (size_t)recv_len;

        // Calcula e publica progresso
        uint8_t percent = (uint8_t)((received * 100) / content_len);
        ota_progress_post(percent, (uint32_t)received, (uint32_t)content_len, OTA_STATE_RECEIVING);

        // Log a cada 10%
        if (percent % 10 == 0) {
            ESP_LOGI(TAG, "Progresso: %d%% (%u/%u bytes)", percent,
                     (unsigned)received, (unsigned)content_len);
        }
    }

    // Libera buffer
    free(buf);

    // Finaliza SHA-256
    uint8_t computed_sha[32] = {};
    mbedtls_sha256_finish(&sha_ctx, computed_sha);
    mbedtls_sha256_free(&sha_ctx);

    // Verifica SHA-256 se fornecido
    if (s_sha256_provided) {
        if (memcmp(computed_sha, s_expected_sha256, 32) != 0) {
            ESP_LOGE(TAG, "SHA-256 mismatch! Firmware corrompido.");
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SHA-256 mismatch");
            ota_progress_post(100, (uint32_t)received, (uint32_t)content_len, OTA_STATE_FAILED);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "SHA-256 verificado com sucesso");
    } else {
        ESP_LOGW(TAG, "SHA-256 nao fornecido, pulando verificacao de hash");
    }

    // Publica estado VERIFYING
    ota_progress_post(100, (uint32_t)content_len, (uint32_t)content_len, OTA_STATE_VERIFYING);

    // Finaliza OTA (valida estrutura da imagem)
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end falhou: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Validacao de imagem falhou");
        ota_progress_post(100, (uint32_t)content_len, (uint32_t)content_len, OTA_STATE_FAILED);
        return ESP_FAIL;
    }

    // Define particao de boot
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition falhou: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Falha ao definir particao de boot");
        ota_progress_post(100, (uint32_t)content_len, (uint32_t)content_len, OTA_STATE_FAILED);
        return ESP_FAIL;
    }

    // Responde ao cliente
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Reiniciando...\"}");

    // Publica estado REBOOTING
    ota_progress_post(100, (uint32_t)content_len, (uint32_t)content_len, OTA_STATE_REBOOTING);

    ESP_LOGI(TAG, "OTA completo! Reiniciando em %d ms...", OTA_REBOOT_DELAY_MS);

    // Delay antes do reboot para enviar resposta HTTP
    vTaskDelay(pdMS_TO_TICKS(OTA_REBOOT_DELAY_MS));

    // Reinicia dispositivo
    esp_restart();

    // Nunca chega aqui
    return ESP_OK;
}

// ============================================================================
// HANDLER DE STATUS (/status GET)
// ============================================================================

static esp_err_t ota_status_handler(httpd_req_t* req) {
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);

    char response[128];
    if (update_partition) {
        snprintf(response, sizeof(response),
                 "{\"status\":\"ready\",\"partition\":\"%s\",\"max_size\":%lu}",
                 update_partition->label,
                 (unsigned long)update_partition->size);
    } else {
        snprintf(response, sizeof(response),
                 "{\"status\":\"error\",\"message\":\"Particao OTA nao encontrada\"}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

// ============================================================================
// CONTROLE DO SERVIDOR HTTP
// ============================================================================

bool ota_http_server_start(void) {
    if (s_server) {
        ESP_LOGW(TAG, "Servidor HTTP ja iniciado");
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = OTA_HTTP_STACK_SIZE;
    config.server_port = OTA_HTTP_SERVER_PORT;
    config.max_open_sockets = 1;
    config.recv_wait_timeout = OTA_HTTP_RECV_TIMEOUT_S;
    config.send_wait_timeout = 10;
    config.core_id = 0;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar servidor HTTP: %s", esp_err_to_name(err));
        return false;
    }

    // Registra handler /update (POST)
    const httpd_uri_t update_uri = {
        .uri       = "/update",
        .method    = HTTP_POST,
        .handler   = ota_upload_handler,
        .user_ctx  = nullptr
    };
    httpd_register_uri_handler(s_server, &update_uri);

    // Registra handler /status (GET)
    const httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = ota_status_handler,
        .user_ctx  = nullptr
    };
    httpd_register_uri_handler(s_server, &status_uri);

    ESP_LOGI(TAG, "Servidor HTTP OTA iniciado na porta %d", OTA_HTTP_SERVER_PORT);
    return true;
}

void ota_http_server_stop(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = nullptr;
        ESP_LOGI(TAG, "Servidor HTTP OTA parado");
    }

    // Libera fila de progresso
    if (s_ota_progress_queue) {
        vQueueDelete(s_ota_progress_queue);
        s_ota_progress_queue = nullptr;
    }
}
