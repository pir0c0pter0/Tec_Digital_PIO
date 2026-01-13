/**
 * ============================================================================
 * INICIALIZACAO DA APLICACAO
 * ============================================================================
 *
 * Modulo central de inicializacao do sistema.
 * Coordena a inicializacao de todos os subsistemas.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "core/app_init.h"
#include "config/app_config.h"
#include "utils/debug_utils.h"
#include "utils/time_utils.h"

#include "esp_log.h"
#include "esp_littlefs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <dirent.h>
#include <stdio.h>

LOG_TAG("APP_INIT");

// ============================================================================
// INICIALIZACAO DO FILESYSTEM
// ============================================================================

static esp_err_t init_filesystem(void) {
    LOG_I(TAG, "Inicializando LittleFS...");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = FS_BASE_PATH,
        .partition_label = FS_PARTITION_LABEL,
        .format_if_mount_failed = FS_FORMAT_IF_FAILED,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            LOG_E(TAG, "Falha ao montar ou formatar filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            LOG_E(TAG, "Particao nao encontrada");
        } else {
            LOG_E(TAG, "Falha ao inicializar LittleFS: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        LOG_I(TAG, "LittleFS montado! Total: %d bytes, Usado: %d bytes", (int)total, (int)used);
    }

    return ESP_OK;
}

static void list_filesystem_files(void) {
    LOG_I(TAG, "Listando arquivos...");

    DIR* dir = opendir(FS_BASE_PATH);
    if (!dir) {
        LOG_E(TAG, "Erro ao abrir diretorio raiz");
        return;
    }

    struct dirent* entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            LOG_I(TAG, "  Arquivo: %s", entry->d_name);
            count++;
        }
    }

    closedir(dir);

    if (count == 0) {
        LOG_W(TAG, "Nenhum arquivo encontrado");
    } else {
        LOG_I(TAG, "Total de arquivos: %d", count);
    }
}

// ============================================================================
// FUNCOES PUBLICAS
// ============================================================================

extern "C" {

bool app_init_filesystem(void) {
    if (init_filesystem() != ESP_OK) {
        return false;
    }

    list_filesystem_files();
    return true;
}

void app_print_version(void) {
    LOG_I(TAG, "=================================");
    LOG_I(TAG, "%s", APP_NAME);
    LOG_I(TAG, "Versao: %s", APP_VERSION_STRING);
    LOG_I(TAG, "Desenvolvido por: %s", APP_AUTHOR);
    LOG_I(TAG, "Copyright: %s", APP_COMPANY);
    LOG_I(TAG, "=================================");
}

void app_print_system_info(void) {
    LOG_I(TAG, "=== Informacoes do Sistema ===");
    LOG_I(TAG, "Display: %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    LOG_I(TAG, "Grid: %dx%d botoes", GRID_COLS, GRID_ROWS);
    LOG_I(TAG, "Max motoristas: %d", MAX_MOTORISTAS);

    debug_print_heap_info();
}

} // extern "C"
