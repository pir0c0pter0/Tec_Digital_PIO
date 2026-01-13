/**
 * ============================================================================
 * TEMA DA UI
 * ============================================================================
 *
 * Define cores, fontes e estilos padronizados para a interface.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "ui/common/theme.h"
#include "config/app_config.h"

// ============================================================================
// INSTANCIA SINGLETON
// ============================================================================

Theme* Theme::instance = nullptr;

Theme* Theme::getInstance() {
    if (instance == nullptr) {
        instance = new Theme();
    }
    return instance;
}

Theme::Theme() {
    initColors();
}

// ============================================================================
// INICIALIZACAO
// ============================================================================

void Theme::initColors() {
    // Cores de fundo
    bgPrimary = lv_color_hex(THEME_BG_PRIMARY);
    bgSecondary = lv_color_hex(THEME_BG_SECONDARY);
    bgTertiary = lv_color_hex(THEME_BG_TERTIARY);

    // Cores de status
    colorSuccess = lv_color_hex(THEME_COLOR_SUCCESS);
    colorWarning = lv_color_hex(THEME_COLOR_WARNING);
    colorError = lv_color_hex(THEME_COLOR_ERROR);
    colorInfo = lv_color_hex(THEME_COLOR_INFO);

    // Cores de texto
    textPrimary = lv_color_hex(THEME_TEXT_PRIMARY);
    textSecondary = lv_color_hex(THEME_TEXT_SECONDARY);
    textMuted = lv_color_hex(THEME_TEXT_MUTED);
}

// ============================================================================
// CORES POR ESTADO DE JORNADA
// ============================================================================

lv_color_t Theme::getColorForJornadaState(int state) {
    switch (state) {
        case 0: return lv_color_hex(0x666666);       // INATIVO - Cinza
        case 1: return lv_color_hex(THEME_BTN_JORNADA);   // JORNADA
        case 2: return lv_color_hex(THEME_BTN_MANOBRA);   // MANOBRA
        case 3: return lv_color_hex(THEME_BTN_REFEICAO);  // REFEICAO
        case 4: return lv_color_hex(THEME_COLOR_WARNING); // ESPERA
        case 5: return lv_color_hex(THEME_BTN_DESCARGA);  // DESCARGA
        case 6: return lv_color_hex(THEME_COLOR_INFO);    // ABASTECIMENTO
        default: return lv_color_hex(0x666666);
    }
}

lv_color_t Theme::getColorForAction(int action) {
    switch (action) {
        case 0:  return lv_color_hex(THEME_BTN_JORNADA);      // JORNADA
        case 1:  return lv_color_hex(THEME_BTN_REFEICAO);     // REFEICAO
        case 2:  return lv_color_hex(THEME_COLOR_INFO);       // ESPERA
        case 3:  return lv_color_hex(THEME_BTN_MANOBRA);      // MANOBRA
        case 4:  return lv_color_hex(THEME_BTN_CARGA);        // CARGA
        case 5:  return lv_color_hex(THEME_BTN_DESCARGA);     // DESCARGA
        case 6:  return lv_color_hex(THEME_COLOR_INFO);       // ABASTECER
        case 7:  return lv_color_hex(THEME_COLOR_INFO);       // DESCANSAR
        case 8:  return lv_color_hex(THEME_BTN_EMERGENCIA);   // TRANSITO
        case 9:  return lv_color_hex(THEME_BTN_EMERGENCIA);   // POLICIA
        case 10: return lv_color_hex(THEME_BTN_EMERGENCIA);   // PANE
        case 11: return lv_color_hex(THEME_BTN_EMERGENCIA);   // EMERGENCIA
        default: return lv_color_hex(0x666666);
    }
}

// ============================================================================
// APLICAR ESTILOS
// ============================================================================

void Theme::applyButtonStyle(lv_obj_t* btn, lv_color_t bgColor) {
    if (!btn) return;

    lv_obj_set_style_bg_color(btn, bgColor, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
}

void Theme::applyLabelStyle(lv_obj_t* label, lv_color_t textColor, const lv_font_t* font) {
    if (!label) return;

    lv_obj_set_style_text_color(label, textColor, LV_PART_MAIN);
    if (font) {
        lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    }
}

void Theme::applyContainerStyle(lv_obj_t* container, lv_color_t bgColor) {
    if (!container) return;

    lv_obj_set_style_bg_color(container, bgColor, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
}

void Theme::applyPopupStyle(lv_obj_t* popup) {
    if (!popup) return;

    lv_obj_set_style_bg_color(popup, bgTertiary, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(popup, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(popup, textPrimary, LV_PART_MAIN);
    lv_obj_set_style_radius(popup, 15, LV_PART_MAIN);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);
}

// ============================================================================
// INTERFACE C
// ============================================================================

extern "C" {

lv_color_t theme_get_bg_primary(void) {
    return Theme::getInstance()->getBgPrimary();
}

lv_color_t theme_get_bg_secondary(void) {
    return Theme::getInstance()->getBgSecondary();
}

lv_color_t theme_get_color_success(void) {
    return Theme::getInstance()->getColorSuccess();
}

lv_color_t theme_get_color_error(void) {
    return Theme::getInstance()->getColorError();
}

lv_color_t theme_get_text_primary(void) {
    return Theme::getInstance()->getTextPrimary();
}

lv_color_t theme_get_jornada_color(int state) {
    return Theme::getInstance()->getColorForJornadaState(state);
}

} // extern "C"
