#ifndef LVGL_FS_DRIVER_H
#define LVGL_FS_DRIVER_H

#include <lvgl.h>

/**
 * @brief Inicializa e registra o driver do LittleFS para a LVGL.
 *
 * Esta função deve ser chamada uma vez no setup(), após LittleFS.begin() e lv_init().
 * @param drive_letter A letra do drive a ser associada (ex: 'A').
 */
void lvgl_fs_init(char drive_letter);

#endif // LVGL_FS_DRIVER_H