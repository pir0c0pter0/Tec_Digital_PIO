/**
 * ============================================================================
 * SPLASH SCREEN SIMPLIFICADO
 * ============================================================================
 * 
 * Tela de inicialização que mostra uma imagem ou logo durante o boot
 * antes de carregar o sistema de ignição.
 * 
 * ============================================================================
 */

#ifndef SIMPLE_SPLASH_H
#define SIMPLE_SPLASH_H

#include <lvgl.h>

// ============================================================================
// CONFIGURAÇÕES
// ============================================================================

// Tempo de exibição do splash em ms (4 segundos)
#define SPLASH_DURATION_MS 1000

// ============================================================================
// VARIÁVEIS EXTERNAS
// ============================================================================

extern bool splash_done;
extern lv_obj_t* splash_screen;
extern lv_timer_t* splash_timer;

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

/**
 * Cria e exibe a tela de splash
 * Pode carregar imagem da flash ou mostrar logo/texto
 */
void createSplashScreen();

/**
 * Remove a splash screen e libera recursos
 * Chamada automaticamente após SPLASH_DURATION_MS
 */
void removeSplashScreen();

/**
 * Verifica se o splash terminou
 * @return true se splash concluído, false caso contrário
 */
bool isSplashDone();

#endif // SIMPLE_SPLASH_H