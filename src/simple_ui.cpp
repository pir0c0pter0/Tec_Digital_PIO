/**
 * ============================================================================
 * INTERFACE SIMPLES - IMPLEMENTAÇÃO (CORRIGIDA - THREAD-SAFE)
 * ============================================================================
 */

#include "simple_ui.h"
#include "esp_bsp.h"

// Declaração externa para tocar áudio
extern "C" void playAudioFile(const char* filename);

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

lv_obj_t* screen_main = NULL;
lv_obj_t* indicator_ignicao = NULL;
lv_obj_t* label_status = NULL;
lv_obj_t* label_message = NULL;
lv_timer_t* message_timer = NULL;
lv_timer_t* blink_timer = NULL;

// Estrutura para dados do timer de piscar
typedef struct {
    lv_obj_t* indicator;
    lv_color_t original_color;
} blink_data_t;

// ============================================================================
// CALLBACKS INTERNOS
// ============================================================================

/**
 * Callback para restaurar cor original após piscar
 */
static void restore_color_cb(lv_timer_t* timer) {
    blink_data_t* data = (blink_data_t*)timer->user_data;
    
    if (data && data->indicator) {
        lv_obj_set_style_bg_color(data->indicator, data->original_color, LV_PART_MAIN);
    }
    
    // Libera memória e deleta timer
    if (data) {
        free(data);
    }
    blink_timer = NULL;
    lv_timer_del(timer);
}

/**
 * Callback quando o indicador é tocado
 */
static void indicator_clicked_cb(lv_event_t* e) {
    static int clickCount = 0;
    clickCount++;
    
    lv_obj_t* ind = lv_event_get_target(e);
    
    // Mostra mensagem
    char msg[100];
    snprintf(msg, sizeof(msg), "Toque detectado! Contador: %d", clickCount);
    showMessage(msg, 3000);
    
    // Feedback visual - pisca o indicador
    lv_color_t originalColor = lv_obj_get_style_bg_color(ind, LV_PART_MAIN);
    
    // Cancela timer de piscar anterior, se existir
    if (blink_timer) {
        blink_data_t* old_data = (blink_data_t*)blink_timer->user_data;
        if (old_data) {
            free(old_data);
        }
        lv_timer_del(blink_timer);
        blink_timer = NULL;
    }
    
    // Pisca branco rapidamente
    lv_obj_set_style_bg_color(ind, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    
    // Prepara dados para o callback
    blink_data_t* data = (blink_data_t*)malloc(sizeof(blink_data_t));
    if (data) {
        data->indicator = ind;
        data->original_color = originalColor;
        
        // Cria timer para voltar à cor original após 100ms
        blink_timer = lv_timer_create(restore_color_cb, 100, data);
        lv_timer_set_repeat_count(blink_timer, 1);
    }
}

/**
 * Callback para esconder mensagem temporária
 */
static void hide_message_cb(lv_timer_t* timer) {
    if (label_message) {
        lv_obj_add_flag(label_message, LV_OBJ_FLAG_HIDDEN);
    }
    if (message_timer) {
        lv_timer_del(message_timer);
        message_timer = NULL;
    }
}

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

/**
 * Cria a interface simples
 */
void createSimpleUI() {
    if (bsp_display_lock(0)) {
        // --------------------------------------------------------------------------
        // CRIAÇÃO DA TELA PRINCIPAL
        // --------------------------------------------------------------------------
        screen_main = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_main, lv_color_hex(0x000000), LV_PART_MAIN);
        
        // --------------------------------------------------------------------------
        // INDICADOR DE IGNIÇÃO (CÍRCULO NO CANTO SUPERIOR)
        // --------------------------------------------------------------------------
        
        // Container para o indicador
        indicator_ignicao = lv_obj_create(screen_main);
        lv_obj_set_size(indicator_ignicao, 60, 60);
        lv_obj_align(indicator_ignicao, LV_ALIGN_TOP_RIGHT, -10, 10);
        
        // Estilo do indicador - círculo
        lv_obj_set_style_radius(indicator_ignicao, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(indicator_ignicao, 3, LV_PART_MAIN);
        lv_obj_set_style_border_color(indicator_ignicao, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        
        // Cor inicial - vermelho (OFF)
        lv_obj_set_style_bg_color(indicator_ignicao, lv_color_hex(0xFF0000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(indicator_ignicao, LV_OPA_COVER, LV_PART_MAIN);
        
        // Remove scrollbar
        lv_obj_clear_flag(indicator_ignicao, LV_OBJ_FLAG_SCROLLABLE);
        
        // Adiciona capacidade de clique
        lv_obj_add_flag(indicator_ignicao, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(indicator_ignicao, indicator_clicked_cb, LV_EVENT_CLICKED, NULL);
        
        // --------------------------------------------------------------------------
        // LABEL DE STATUS (ON/OFF) DENTRO DO INDICADOR
        // --------------------------------------------------------------------------
        label_status = lv_label_create(indicator_ignicao);
        lv_label_set_text(label_status, "OFF");
        lv_obj_center(label_status);
        lv_obj_set_style_text_color(label_status, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(label_status, &lv_font_montserrat_16, LV_PART_MAIN);
        
        // --------------------------------------------------------------------------
        // LABEL PARA MENSAGENS (CENTRO DA TELA - INICIALMENTE OCULTO)
        // --------------------------------------------------------------------------
        label_message = lv_label_create(screen_main);
        lv_label_set_text(label_message, "");
        lv_obj_center(label_message);
        lv_obj_set_style_text_color(label_message, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(label_message, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_add_flag(label_message, LV_OBJ_FLAG_HIDDEN);
        
        // --------------------------------------------------------------------------
        // TEXTO INFORMATIVO (PARTE INFERIOR)
        // --------------------------------------------------------------------------
        lv_obj_t* label_info = lv_label_create(screen_main);
        lv_label_set_text(label_info, "Sistema de Ignicao");
        lv_obj_align(label_info, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_set_style_text_color(label_info, lv_color_hex(0x808080), LV_PART_MAIN);
        lv_obj_set_style_text_font(label_info, &lv_font_montserrat_14, LV_PART_MAIN);
        
        // --------------------------------------------------------------------------
        // CARREGA A TELA
        // --------------------------------------------------------------------------
        lv_scr_load_anim(screen_main, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
        
        bsp_display_unlock();
        
        esp_rom_printf("Simple UI created and loaded successfully\n");
    }
}

/**
 * Atualiza o indicador de ignição (THREAD-SAFE)
 * CRÍTICO: Não força refresh imediato - deixa o LVGL gerenciar
 */
void updateIgnicaoIndicator(bool isOn) {
    if (!indicator_ignicao || !label_status) {
        return;
    }
    
    // Cancela qualquer timer de piscar ativo
    if (blink_timer) {
        blink_data_t* data = (blink_data_t*)blink_timer->user_data;
        if (data) {
            free(data);
        }
        lv_timer_del(blink_timer);
        blink_timer = NULL;
    }
    
    // Tenta obter lock com timeout razoável
    if (bsp_display_lock(100)) {
        if (isOn) {
            // IGNIÇÃO ON - Verde
            lv_obj_set_style_bg_color(indicator_ignicao, lv_color_hex(0x00FF00), LV_PART_MAIN);
            lv_label_set_text(label_status, "ON");
            lv_obj_set_style_shadow_color(indicator_ignicao, lv_color_hex(0x00FF00), LV_PART_MAIN);
            lv_obj_set_style_shadow_width(indicator_ignicao, 20, LV_PART_MAIN);
            lv_obj_set_style_shadow_spread(indicator_ignicao, 5, LV_PART_MAIN);
        } else {
            // IGNIÇÃO OFF - Vermelho
            lv_obj_set_style_bg_color(indicator_ignicao, lv_color_hex(0xFF0000), LV_PART_MAIN);
            lv_label_set_text(label_status, "OFF");
            lv_obj_set_style_shadow_width(indicator_ignicao, 0, LV_PART_MAIN);
        }
        
        // CRÍTICO: Removido lv_refr_now() - deixa o LVGL gerenciar o refresh
        // O lv_timer_handler() no loop principal cuidará da atualização
        
        bsp_display_unlock();
    }
}

/**
 * Mostra mensagem temporária
 */
void showMessage(const char* message, uint32_t duration) {
    if (!label_message) return;
    
    if (bsp_display_lock(100)) {
        // Define o texto
        lv_label_set_text(label_message, message);
        
        // Mostra o label
        lv_obj_clear_flag(label_message, LV_OBJ_FLAG_HIDDEN);
        
        // Se já existe um timer, cancela
        if (message_timer) {
            lv_timer_del(message_timer);
            message_timer = NULL;
        }
        
        // Se duration > 0, cria timer para esconder
        if (duration > 0) {
            message_timer = lv_timer_create(hide_message_cb, duration, NULL);
            lv_timer_set_repeat_count(message_timer, 1);
        }
        
        bsp_display_unlock();
    }
}

/**
 * Habilita/desabilita touch
 */
void setTouchEnabled(bool enable) {
    if (!indicator_ignicao) return;
    
    if (bsp_display_lock(100)) {
        if (enable) {
            lv_obj_add_flag(indicator_ignicao, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_clear_flag(indicator_ignicao, LV_OBJ_FLAG_CLICKABLE);
        }
        bsp_display_unlock();
    }
}

/**
 * Testa reprodução de áudio manualmente
 */
void testAudioPlayback(bool ignOn) {
    if (ignOn) {
        showMessage("Testando: Ignição ON", 2000);
        playAudioFile("/ign_on_jornada_manobra.mp3");
    } else {
        showMessage("Testando: Ignição OFF", 2000);
        playAudioFile("/ign_off.mp3");
    }
}

// ============================================================================
// FIM DA IMPLEMENTAÇÃO
// ============================================================================