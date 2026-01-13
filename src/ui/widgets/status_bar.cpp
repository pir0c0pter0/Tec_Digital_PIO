/**
 * ============================================================================
 * WIDGET BARRA DE STATUS
 * ============================================================================
 *
 * Componente reutilizavel para barra de status.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "ui/widgets/status_bar.h"
#include "ui/common/theme.h"
#include "config/app_config.h"
#include "utils/time_utils.h"
#include "esp_bsp.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// CONSTRUTOR E DESTRUTOR
// ============================================================================

StatusBar::StatusBar()
    : container(nullptr)
    , ignicaoIndicator(nullptr)
    , ignicaoLabel(nullptr)
    , tempoIgnicaoLabel(nullptr)
    , tempoJornadaLabel(nullptr)
    , mensagemLabel(nullptr)
    , updateTimer(nullptr)
    , messageExpireTime(0)
    , parent(nullptr)
{
}

StatusBar::~StatusBar() {
    destroy();
}

// ============================================================================
// CRIACAO
// ============================================================================

void StatusBar::create(lv_obj_t* parentObj) {
    if (container) {
        destroy();
    }

    parent = parentObj;

    if (!bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        return;
    }

    Theme* theme = Theme::getInstance();

    // Container principal
    container = lv_obj_create(parent);
    lv_obj_set_size(container, DISPLAY_WIDTH, STATUS_BAR_HEIGHT);
    lv_obj_align(container, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 2, LV_PART_MAIN);
    lv_obj_set_style_border_side(container, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_color(container, lv_color_hex(0x4a4a4a), LV_PART_MAIN);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    // Indicador de ignicao (circulo)
    ignicaoIndicator = lv_obj_create(container);
    lv_obj_set_size(ignicaoIndicator, 30, 30);
    lv_obj_align(ignicaoIndicator, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_radius(ignicaoIndicator, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ignicaoIndicator, theme->getColorError(), LV_PART_MAIN);
    lv_obj_set_style_border_width(ignicaoIndicator, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(ignicaoIndicator, theme->getTextPrimary(), LV_PART_MAIN);
    lv_obj_clear_flag(ignicaoIndicator, LV_OBJ_FLAG_SCROLLABLE);

    // Label dentro do indicador
    ignicaoLabel = lv_label_create(ignicaoIndicator);
    lv_label_set_text(ignicaoLabel, "OFF");
    lv_obj_center(ignicaoLabel);
    lv_obj_set_style_text_color(ignicaoLabel, theme->getTextPrimary(), LV_PART_MAIN);
    lv_obj_set_style_text_font(ignicaoLabel, &lv_font_montserrat_10, LV_PART_MAIN);

    // Tempo de ignicao
    tempoIgnicaoLabel = lv_label_create(container);
    lv_label_set_text(tempoIgnicaoLabel, "");
    lv_obj_align(tempoIgnicaoLabel, LV_ALIGN_LEFT_MID, 55, 0);
    lv_obj_set_style_text_color(tempoIgnicaoLabel, theme->getTextSecondary(), LV_PART_MAIN);
    lv_obj_set_style_text_font(tempoIgnicaoLabel, &lv_font_montserrat_12, LV_PART_MAIN);

    // Tempo de jornada
    tempoJornadaLabel = lv_label_create(container);
    lv_label_set_text(tempoJornadaLabel, "");
    lv_obj_align(tempoJornadaLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(tempoJornadaLabel, theme->getTextSecondary(), LV_PART_MAIN);
    lv_obj_set_style_text_font(tempoJornadaLabel, &lv_font_montserrat_12, LV_PART_MAIN);

    // Mensagem central
    mensagemLabel = lv_label_create(container);
    lv_label_set_text(mensagemLabel, "");
    lv_obj_align(mensagemLabel, LV_ALIGN_CENTER, -30, 0);
    lv_obj_set_width(mensagemLabel, 250);
    lv_label_set_long_mode(mensagemLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(mensagemLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(mensagemLabel, theme->getTextMuted(), LV_PART_MAIN);
    lv_obj_set_style_text_font(mensagemLabel, &lv_font_montserrat_20, LV_PART_MAIN);

    // Timer de atualizacao
    updateTimer = lv_timer_create(updateTimerCallback, STATUS_BAR_UPDATE_MS, this);

    bsp_display_unlock();
}

void StatusBar::destroy() {
    if (updateTimer) {
        lv_timer_del(updateTimer);
        updateTimer = nullptr;
    }

    if (container && bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        lv_obj_del(container);
        container = nullptr;
        ignicaoIndicator = nullptr;
        ignicaoLabel = nullptr;
        tempoIgnicaoLabel = nullptr;
        tempoJornadaLabel = nullptr;
        mensagemLabel = nullptr;
        bsp_display_unlock();
    }
}

// ============================================================================
// ATUALIZACAO
// ============================================================================

void StatusBar::update(const StatusBarData& data) {
    if (!container) return;

    if (!bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        return;
    }

    Theme* theme = Theme::getInstance();

    // Atualizar indicador de ignicao
    if (ignicaoIndicator && ignicaoLabel) {
        if (data.ignicaoOn) {
            lv_obj_set_style_bg_color(ignicaoIndicator, theme->getColorSuccess(), LV_PART_MAIN);
            lv_label_set_text(ignicaoLabel, "ON");
        } else {
            lv_obj_set_style_bg_color(ignicaoIndicator, theme->getColorError(), LV_PART_MAIN);
            lv_label_set_text(ignicaoLabel, "OFF");
        }
    }

    // Atualizar tempo de ignicao
    if (tempoIgnicaoLabel) {
        if (data.ignicaoOn && data.tempoIgnicao > 0) {
            char buffer[48];
            char timeStr[TIME_FORMAT_MIN_BUFFER];
            time_format_ms(data.tempoIgnicao, timeStr, sizeof(timeStr));
            snprintf(buffer, sizeof(buffer), "Ignicao: %s", timeStr);
            lv_label_set_text(tempoIgnicaoLabel, buffer);
        } else {
            lv_label_set_text(tempoIgnicaoLabel, "");
        }
    }

    // Atualizar tempo de jornada
    if (tempoJornadaLabel) {
        if (data.tempoJornada > 0) {
            char buffer[48];
            char timeStr[TIME_FORMAT_MIN_BUFFER];
            time_format_ms(data.tempoJornada, timeStr, sizeof(timeStr));
            snprintf(buffer, sizeof(buffer), "Jornada M1: %s", timeStr);
            lv_label_set_text(tempoJornadaLabel, buffer);
        } else {
            lv_label_set_text(tempoJornadaLabel, "");
        }
    }

    // Atualizar mensagem extra
    if (mensagemLabel && data.mensagem && strlen(data.mensagem) > 0) {
        lv_label_set_text(mensagemLabel, data.mensagem);
    }

    bsp_display_unlock();
}

void StatusBar::setIgnicao(bool on, uint32_t tempo) {
    StatusBarData data = {
        .ignicaoOn = on,
        .tempoIgnicao = tempo,
        .tempoJornada = 0,
        .mensagem = nullptr
    };
    update(data);
}

void StatusBar::setMessage(const char* message, lv_color_t color, const lv_font_t* font, uint32_t timeoutMs) {
    if (!mensagemLabel) return;

    if (!bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        return;
    }

    lv_label_set_text(mensagemLabel, message ? message : "");
    lv_obj_set_style_text_color(mensagemLabel, color, LV_PART_MAIN);
    if (font) {
        lv_obj_set_style_text_font(mensagemLabel, font, LV_PART_MAIN);
    }

    if (timeoutMs > 0 && message && strlen(message) > 0) {
        messageExpireTime = time_millis() + timeoutMs;
    } else {
        messageExpireTime = 0;
    }

    bsp_display_unlock();
}

void StatusBar::clearMessage() {
    setMessage("", lv_color_hex(THEME_TEXT_MUTED), &lv_font_montserrat_20, 0);
    messageExpireTime = 0;
}

// ============================================================================
// TIMER CALLBACK
// ============================================================================

void StatusBar::updateTimerCallback(lv_timer_t* timer) {
    StatusBar* self = static_cast<StatusBar*>(timer->user_data);
    if (!self) return;

    // Verificar expiracao de mensagem
    if (self->messageExpireTime > 0 && time_millis() >= self->messageExpireTime) {
        self->clearMessage();
    }
}
