/**
 * ============================================================================
 * DRIVER DE FILESYSTEM LVGL - ESP-IDF (POSIX)
 * ============================================================================
 */

#include "lvgl_fs_driver.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "FS_DRV";

// ============================================================================
// FUNÇÕES DE CALLBACK POSIX PARA LVGL
// ============================================================================

/**
 * @brief Tenta abrir um arquivo. Chamado pela LVGL.
 */
static void* fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
    const char *fs_mode;
    if (mode == LV_FS_MODE_WR) {
        fs_mode = "wb";
    } else if (mode == LV_FS_MODE_RD) {
        fs_mode = "rb";
    } else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) {
        fs_mode = "r+b";
    } else {
        return NULL;
    }

    // O caminho da LVGL vem como "A:/imagem.png"
    // Construir o caminho completo para LittleFS
    char full_path[256];
    const char* fs_path = path;
    if (fs_path[1] == ':') {
        fs_path += 2; // Pula "A:"
    }

    snprintf(full_path, sizeof(full_path), "/littlefs%s", fs_path);

    FILE* fp = fopen(full_path, fs_mode);
    if (!fp) {
        ESP_LOGW(TAG, "Falha ao abrir: %s", full_path);
        return NULL;
    }

    ESP_LOGI(TAG, "Arquivo aberto: %s", full_path);
    return fp;
}

/**
 * @brief Fecha um arquivo. Chamado pela LVGL.
 */
static lv_fs_res_t fs_close_cb(lv_fs_drv_t *drv, void *file_p) {
    FILE* fp = (FILE*)file_p;
    fclose(fp);
    return LV_FS_RES_OK;
}

/**
 * @brief Lê dados de um arquivo. Chamado pela LVGL.
 */
static lv_fs_res_t fs_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br) {
    FILE* fp = (FILE*)file_p;
    *br = fread(buf, 1, btr, fp);
    return LV_FS_RES_OK;
}

/**
 * @brief Move o cursor de leitura. Chamado pela LVGL.
 */
static lv_fs_res_t fs_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence) {
    FILE* fp = (FILE*)file_p;
    int origin;

    if (whence == LV_FS_SEEK_SET) origin = SEEK_SET;
    else if (whence == LV_FS_SEEK_CUR) origin = SEEK_CUR;
    else if (whence == LV_FS_SEEK_END) origin = SEEK_END;
    else return LV_FS_RES_INV_PARAM;

    if (fseek(fp, pos, origin) == 0) {
        return LV_FS_RES_OK;
    }
    return LV_FS_RES_UNKNOWN;
}

/**
 * @brief Informa a posição atual do cursor. Chamado pela LVGL.
 */
static lv_fs_res_t fs_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
    FILE* fp = (FILE*)file_p;
    *pos_p = ftell(fp);
    return LV_FS_RES_OK;
}

// ============================================================================
// FUNÇÃO DE INICIALIZAÇÃO DO DRIVER
// ============================================================================

void lvgl_fs_init(char drive_letter) {
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = drive_letter;
    fs_drv.open_cb = fs_open_cb;
    fs_drv.close_cb = fs_close_cb;
    fs_drv.read_cb = fs_read_cb;
    fs_drv.seek_cb = fs_seek_cb;
    fs_drv.tell_cb = fs_tell_cb;

    lv_fs_drv_register(&fs_drv);

    ESP_LOGI(TAG, "Driver LittleFS para LVGL registrado na letra '%c'", drive_letter);
}
