/**
 * ============================================================================
 * SPLASH SCREEN SIMPLIFICADO - IMPLEMENTAÇÃO
 * ============================================================================
 */

#include "simple_splash.h"
#include "esp_bsp.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "SPLASH";

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

bool splash_done = false;
lv_obj_t* splash_screen = NULL;
lv_timer_t* splash_timer = NULL;

// ============================================================================
// FUNÇÕES INTERNAS
// ============================================================================

/**
 * Callback do timer - chamado quando o tempo do splash expira
 */
static void splash_timer_cb(lv_timer_t* timer) {
    removeSplashScreen();
}

/**
 * Tenta carregar imagem BMP da flash
 */
static bool loadSplashImage(lv_obj_t* parent) {
    // Verifica se existe o arquivo splash.bmp na flash usando POSIX
    FILE* fp = fopen("/littlefs/splash.bmp", "rb");
    if (!fp) {
        esp_rom_printf("Splash image not found in flash\n");
        return false;
    }
    fclose(fp);

    // Aqui você pode implementar carregamento de BMP se necessário
    // Por enquanto, retorna false para usar o splash de texto
    return false;
}

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

/**
 * Cria a tela de splash
 */
void createSplashScreen() {
    if (bsp_display_lock(0)) {
        splash_done = false;
        
        // --------------------------------------------------------------------------
        // CRIA A TELA DE SPLASH
        // --------------------------------------------------------------------------
        splash_screen = lv_obj_create(NULL);
        
        // Fundo gradiente (azul escuro para preto)
        lv_obj_set_style_bg_color(splash_screen, lv_color_hex(0x001122), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(splash_screen, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(splash_screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
        
        // --------------------------------------------------------------------------
        // TENTA CARREGAR IMAGEM OU USA TEXTO
        // --------------------------------------------------------------------------
        if (!loadSplashImage(splash_screen)) {
            // Se não tem imagem, cria splash com texto e animação
            
            // Logo/Título principal
            lv_obj_t* label_title = lv_label_create(splash_screen);
            lv_label_set_text(label_title, "SISTEMA DE IGNICAO");  // Sem acentos
            lv_obj_align(label_title, LV_ALIGN_CENTER, 0, -40);
            lv_obj_set_style_text_color(label_title, lv_color_hex(0x00FF00), LV_PART_MAIN);
            lv_obj_set_style_text_font(label_title, &lv_font_montserrat_28, LV_PART_MAIN);
            
            // Subtítulo
            lv_obj_t* label_subtitle = lv_label_create(splash_screen);
            lv_label_set_text(label_subtitle, "ESP32-S3");
            lv_obj_align(label_subtitle, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_text_color(label_subtitle, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_font(label_subtitle, &lv_font_montserrat_20, LV_PART_MAIN);
            
            // Versão
            lv_obj_t* label_version = lv_label_create(splash_screen);
            lv_label_set_text(label_version, "v1.0.0");
            lv_obj_align(label_version, LV_ALIGN_CENTER, 0, 30);
            lv_obj_set_style_text_color(label_version, lv_color_hex(0x808080), LV_PART_MAIN);
            lv_obj_set_style_text_font(label_version, &lv_font_montserrat_14, LV_PART_MAIN);
            
            // --------------------------------------------------------------------------
            // BARRA DE PROGRESSO ANIMADA
            // --------------------------------------------------------------------------
            lv_obj_t* loading_bar = lv_bar_create(splash_screen);
            lv_obj_set_size(loading_bar, 200, 6);
            lv_obj_align(loading_bar, LV_ALIGN_BOTTOM_MID, 0, -60);
            
            // Estilo da barra
            lv_obj_set_style_bg_color(loading_bar, lv_color_hex(0x202020), LV_PART_MAIN);
            lv_obj_set_style_bg_color(loading_bar, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
            lv_obj_set_style_radius(loading_bar, 3, LV_PART_MAIN);
            lv_obj_set_style_anim_time(loading_bar, SPLASH_DURATION_MS, LV_PART_MAIN);
            
            // Anima a barra de 0 a 100%
            lv_bar_set_value(loading_bar, 0, LV_ANIM_OFF);
            lv_bar_set_value(loading_bar, 100, LV_ANIM_ON);
            
            // --------------------------------------------------------------------------
            // TEXTO "CARREGANDO..."
            // --------------------------------------------------------------------------
            lv_obj_t* label_loading = lv_label_create(splash_screen);
            lv_label_set_text(label_loading, "Inicializando...");
            lv_obj_align(label_loading, LV_ALIGN_BOTTOM_MID, 0, -30);
            lv_obj_set_style_text_color(label_loading, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
            lv_obj_set_style_text_font(label_loading, &lv_font_montserrat_14, LV_PART_MAIN);
            
            // Animação de fade in para o título
            lv_anim_t anim_fade;
            lv_anim_init(&anim_fade);
            lv_anim_set_var(&anim_fade, label_title);
            lv_anim_set_values(&anim_fade, 0, 255);
            lv_anim_set_time(&anim_fade, 1000);
            lv_anim_set_exec_cb(&anim_fade, [](void* obj, int32_t value) {
                lv_obj_set_style_opa((lv_obj_t*)obj, value, LV_PART_MAIN);
            });
            lv_anim_start(&anim_fade);
        }
        
        // --------------------------------------------------------------------------
        // ADICIONA INDICADOR DE IGNIÇÃO (PEQUENO)
        // --------------------------------------------------------------------------
        lv_obj_t* mini_indicator = lv_obj_create(splash_screen);
        lv_obj_set_size(mini_indicator, 10, 10);
        lv_obj_align(mini_indicator, LV_ALIGN_TOP_RIGHT, -10, 10);
        lv_obj_set_style_radius(mini_indicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(mini_indicator, lv_color_hex(0xFF0000), LV_PART_MAIN);
        lv_obj_set_style_border_width(mini_indicator, 0, LV_PART_MAIN);
        
        // Animação de pulso no indicador
        lv_anim_t anim_pulse;
        lv_anim_init(&anim_pulse);
        lv_anim_set_var(&anim_pulse, mini_indicator);
        lv_anim_set_values(&anim_pulse, 10, 15);
        lv_anim_set_time(&anim_pulse, 1000);
        lv_anim_set_playback_time(&anim_pulse, 1000);
        lv_anim_set_repeat_count(&anim_pulse, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&anim_pulse, [](void* obj, int32_t value) {
            lv_obj_set_size((lv_obj_t*)obj, value, value);
            lv_obj_align((lv_obj_t*)obj, LV_ALIGN_TOP_RIGHT, -10, 10);
        });
        lv_anim_start(&anim_pulse);
        
        // --------------------------------------------------------------------------
        // CARREGA E INICIA TIMER
        // --------------------------------------------------------------------------
        lv_scr_load(splash_screen);
        
        // Cria timer para remover splash após SPLASH_DURATION_MS
        splash_timer = lv_timer_create(splash_timer_cb, SPLASH_DURATION_MS, NULL);
        lv_timer_set_repeat_count(splash_timer, 1); // Executa apenas uma vez
        
        bsp_display_unlock();
        
        esp_rom_printf("Splash screen created (duration: %dms)\n", SPLASH_DURATION_MS);
    }
}

/**
 * Remove a splash screen
 */
void removeSplashScreen() {
    if (splash_screen && !splash_done) {  // Adiciona verificação dupla
        if (bsp_display_lock(0)) {
            // Marca como concluído ANTES de deletar
            splash_done = true;
            
            // NÃO deleta a tela ativa - deixa para o createSimpleUI substituir
            // Apenas limpa a referência
            splash_screen = NULL;
            
            // Deleta o timer se ainda existir
            if (splash_timer) {
                lv_timer_del(splash_timer);
                splash_timer = NULL;
            }
            
            bsp_display_unlock();
            
            esp_rom_printf("Splash screen marked for removal\n");
        }
    }
}

/**
 * Verifica se splash terminou
 */
bool isSplashDone() {
    return splash_done;
}

// ============================================================================
// FIM DA IMPLEMENTAÇÃO
// ============================================================================