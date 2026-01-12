/**
 * ============================================================================
 * INTERFACE SIMPLES - INDICADOR DE IGNIÇÃO
 * ============================================================================
 * 
 * Interface minimalista com apenas um indicador de status da ignição
 * no canto superior da tela.
 * 
 * ============================================================================
 */

#ifndef SIMPLE_UI_H
#define SIMPLE_UI_H

#include <lvgl.h>

// ============================================================================
// VARIÁVEIS GLOBAIS DA UI
// ============================================================================

extern lv_obj_t* screen_main;
extern lv_obj_t* indicator_ignicao;
extern lv_obj_t* label_status;

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

/**
 * Cria a interface simples com indicador de ignição
 */
void createSimpleUI();

/**
 * Atualiza o indicador de ignição
 * @param isOn true = verde (ON), false = vermelho (OFF)
 */
void updateIgnicaoIndicator(bool isOn);

/**
 * Mostra mensagem temporária na tela (opcional)
 * @param message Mensagem a exibir
 * @param duration Duração em ms (0 = permanente)
 */
void showMessage(const char* message, uint32_t duration = 3000);

/**
 * Habilita/desabilita interação touch
 * @param enable true para habilitar, false para desabilitar
 */
void setTouchEnabled(bool enable);

/**
 * Testa os áudios manualmente (útil para debug)
 * @param ignOn true para testar som de ignição ON, false para OFF
 */
void testAudioPlayback(bool ignOn);

#endif // SIMPLE_UI_H