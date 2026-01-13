/**
 * ============================================================================
 * INICIALIZACAO DA APLICACAO - HEADER
 * ============================================================================
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef APP_INIT_H
#define APP_INIT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa o filesystem (LittleFS)
 * @return true se inicializado com sucesso
 */
bool app_init_filesystem(void);

/**
 * Imprime informacoes de versao
 */
void app_print_version(void);

/**
 * Imprime informacoes do sistema
 */
void app_print_system_info(void);

#ifdef __cplusplus
}
#endif

#endif // APP_INIT_H
