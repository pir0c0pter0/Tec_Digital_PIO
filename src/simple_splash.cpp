/**
 * ============================================================================
 * SPLASH SCREEN - IMAGEM + BARRA DE LOADING AZUL
 * ============================================================================
 */

#include "simple_splash.h"
#include "esp_bsp.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "SPLASH";

// ============================================================================
// VARIAVEIS GLOBAIS
// ============================================================================

bool splash_done = false;
lv_obj_t* splash_screen = NULL;
lv_timer_t* splash_timer = NULL;
static lv_obj_t* loading_bar = NULL;

// ============================================================================
// FUNCOES INTERNAS
// ============================================================================

/**
 * Callback do timer - chamado quando o tempo do splash expira
 */
static void splash_timer_cb(lv_timer_t* timer) {
    removeSplashScreen();
}

// ============================================================================
// FUNCOES PUBLICAS
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

        // Fundo preto
        lv_obj_set_style_bg_color(splash_screen, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(splash_screen, LV_OPA_COVER, LV_PART_MAIN);

        // --------------------------------------------------------------------------
        // LOGO SPLASH (carrega PNG do LittleFS - drive A:)
        // --------------------------------------------------------------------------
        lv_obj_t* img = lv_img_create(splash_screen);
        lv_img_set_src(img, "A:/logo_splash.png");
        lv_obj_align(img, LV_ALIGN_CENTER, 0, -20);  // Centralizado, acima da barra de loading

        // --------------------------------------------------------------------------
        // BARRA DE LOADING AZUL
        // --------------------------------------------------------------------------
        loading_bar = lv_bar_create(splash_screen);
        lv_obj_set_size(loading_bar, 280, 8);
        lv_obj_align(loading_bar, LV_ALIGN_BOTTOM_MID, 0, -40);

        // Estilo da barra - fundo escuro, indicador azul
        lv_obj_set_style_bg_color(loading_bar, lv_color_hex(0x202020), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(loading_bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(loading_bar, lv_color_hex(0x0066FF), LV_PART_INDICATOR);
        lv_obj_set_style_radius(loading_bar, 4, LV_PART_MAIN);
        lv_obj_set_style_radius(loading_bar, 4, LV_PART_INDICATOR);
        lv_obj_set_style_anim_time(loading_bar, SPLASH_DURATION_MS - 100, LV_PART_MAIN);

        // Anima a barra de 0 a 100%
        lv_bar_set_value(loading_bar, 0, LV_ANIM_OFF);
        lv_bar_set_value(loading_bar, 100, LV_ANIM_ON);

        // --------------------------------------------------------------------------
        // CARREGA E INICIA TIMER
        // --------------------------------------------------------------------------
        lv_scr_load(splash_screen);

        // Cria timer para remover splash apos SPLASH_DURATION_MS
        splash_timer = lv_timer_create(splash_timer_cb, SPLASH_DURATION_MS, NULL);
        lv_timer_set_repeat_count(splash_timer, 1);

        bsp_display_unlock();

        ESP_LOGI(TAG, "Splash screen criado (duracao: %dms)", SPLASH_DURATION_MS);
    }
}

/**
 * Remove a splash screen
 */
void removeSplashScreen() {
    if (splash_screen && !splash_done) {
        if (bsp_display_lock(0)) {
            splash_done = true;
            splash_screen = NULL;
            loading_bar = NULL;

            if (splash_timer) {
                lv_timer_del(splash_timer);
                splash_timer = NULL;
            }

            bsp_display_unlock();

            ESP_LOGI(TAG, "Splash screen removido");
        }
    }
}

/**
 * Verifica se splash terminou
 */
bool isSplashDone() {
    return splash_done;
}
