/**
 * ============================================================================
 * TEMA DA UI - HEADER
 * ============================================================================
 *
 * Define cores, fontes e estilos padronizados para a interface.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

#ifdef __cplusplus

/**
 * Classe singleton para gerenciar o tema da UI
 */
class Theme {
public:
    static Theme* getInstance();

    // Cores de fundo
    lv_color_t getBgPrimary() const { return bgPrimary; }
    lv_color_t getBgSecondary() const { return bgSecondary; }
    lv_color_t getBgTertiary() const { return bgTertiary; }

    // Cores de status
    lv_color_t getColorSuccess() const { return colorSuccess; }
    lv_color_t getColorWarning() const { return colorWarning; }
    lv_color_t getColorError() const { return colorError; }
    lv_color_t getColorInfo() const { return colorInfo; }

    // Cores de texto
    lv_color_t getTextPrimary() const { return textPrimary; }
    lv_color_t getTextSecondary() const { return textSecondary; }
    lv_color_t getTextMuted() const { return textMuted; }

    // Cores por estado/acao
    lv_color_t getColorForJornadaState(int state);
    lv_color_t getColorForAction(int action);

    // Aplicar estilos
    void applyButtonStyle(lv_obj_t* btn, lv_color_t bgColor);
    void applyLabelStyle(lv_obj_t* label, lv_color_t textColor, const lv_font_t* font = nullptr);
    void applyContainerStyle(lv_obj_t* container, lv_color_t bgColor);
    void applyPopupStyle(lv_obj_t* popup);

private:
    Theme();
    void initColors();

    static Theme* instance;

    // Cores de fundo
    lv_color_t bgPrimary;
    lv_color_t bgSecondary;
    lv_color_t bgTertiary;

    // Cores de status
    lv_color_t colorSuccess;
    lv_color_t colorWarning;
    lv_color_t colorError;
    lv_color_t colorInfo;

    // Cores de texto
    lv_color_t textPrimary;
    lv_color_t textSecondary;
    lv_color_t textMuted;
};

extern "C" {
#endif

// Interface C
lv_color_t theme_get_bg_primary(void);
lv_color_t theme_get_bg_secondary(void);
lv_color_t theme_get_color_success(void);
lv_color_t theme_get_color_error(void);
lv_color_t theme_get_text_primary(void);
lv_color_t theme_get_jornada_color(int state);

#ifdef __cplusplus
}
#endif

#endif // UI_THEME_H
