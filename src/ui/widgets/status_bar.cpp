/**
 * ============================================================================
 * WIDGET BARRA DE STATUS
 * ============================================================================
 *
 * Componente reutilizavel para barra de status.
 * Vive em lv_layer_top() para persistir entre transicoes de tela.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "ui/widgets/status_bar.h"
#include "interfaces/i_screen.h"
#include "ui/common/theme.h"
#include "config/app_config.h"
#include "utils/time_utils.h"
#include "esp_bsp.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "STATUS_BAR";

// ============================================================================
// CONSTRUTOR E DESTRUTOR
// ============================================================================

StatusBar::StatusBar()
    : container_(nullptr)
    , swapBtn_(nullptr)
    , ignicaoIndicator_(nullptr)
    , ignicaoLabel_(nullptr)
    , tempoIgnicaoLabel_(nullptr)
    , bleIcon_(nullptr)
    , tempoJornadaLabel_(nullptr)
    , mensagemLabel_(nullptr)
    , updateTimer_(nullptr)
    , messageExpireTime_(0)
    , bleStatus_(BleStatus::DISCONNECTED)
    , screenManager_(nullptr)
{
}

StatusBar::~StatusBar() {
    destroy();
}

// ============================================================================
// CRIACAO (em lv_layer_top())
// ============================================================================

void StatusBar::create() {
    if (container_) {
        destroy();
    }

    if (!bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        ESP_LOGE(TAG, "Falha ao obter lock do display para criar StatusBar");
        return;
    }

    Theme* theme = Theme::getInstance();

    // Container principal em lv_layer_top() (persistente entre telas)
    container_ = lv_obj_create(lv_layer_top());
    lv_obj_set_size(container_, DISPLAY_WIDTH, STATUS_BAR_HEIGHT);
    lv_obj_align(container_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(container_, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(container_, 2, LV_PART_MAIN);
    lv_obj_set_style_border_side(container_, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_color(container_, lv_color_hex(0x4a4a4a), LV_PART_MAIN);
    lv_obj_set_style_pad_all(container_, 0, LV_PART_MAIN);

    // Container nao-clicavel (evita bloquear touch na area da tela abaixo)
    // Botoes individuais serao clicaveis
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Indicador de ignicao (circulo, esquerda) ----
    ignicaoIndicator_ = lv_obj_create(container_);
    lv_obj_set_size(ignicaoIndicator_, 30, 30);
    lv_obj_align(ignicaoIndicator_, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_radius(ignicaoIndicator_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ignicaoIndicator_, theme->getColorError(), LV_PART_MAIN);
    lv_obj_set_style_border_width(ignicaoIndicator_, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(ignicaoIndicator_, theme->getTextPrimary(), LV_PART_MAIN);
    lv_obj_clear_flag(ignicaoIndicator_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ignicaoIndicator_, LV_OBJ_FLAG_CLICKABLE);

    // Label dentro do indicador
    ignicaoLabel_ = lv_label_create(ignicaoIndicator_);
    lv_label_set_text(ignicaoLabel_, "OFF");
    lv_obj_center(ignicaoLabel_);
    lv_obj_set_style_text_color(ignicaoLabel_, theme->getTextPrimary(), LV_PART_MAIN);
    lv_obj_set_style_text_font(ignicaoLabel_, &lv_font_montserrat_10, LV_PART_MAIN);

    // ---- Tempo de ignicao ----
    tempoIgnicaoLabel_ = lv_label_create(container_);
    lv_label_set_text(tempoIgnicaoLabel_, "");
    lv_obj_align(tempoIgnicaoLabel_, LV_ALIGN_LEFT_MID, 42, 0);
    lv_obj_set_style_text_color(tempoIgnicaoLabel_, theme->getTextSecondary(), LV_PART_MAIN);
    lv_obj_set_style_text_font(tempoIgnicaoLabel_, &lv_font_montserrat_12, LV_PART_MAIN);

    // ---- Icone BLE (entre ignicao e centro) ----
    bleIcon_ = lv_label_create(container_);
    lv_label_set_text(bleIcon_, LV_SYMBOL_BLUETOOTH);
    lv_obj_align(bleIcon_, LV_ALIGN_LEFT_MID, 130, 0);
    lv_obj_set_style_text_font(bleIcon_, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(bleIcon_, lv_color_hex(0x666666), LV_PART_MAIN);  // Cinza = desconectado

    // ---- Tempo de jornada ----
    tempoJornadaLabel_ = lv_label_create(container_);
    lv_label_set_text(tempoJornadaLabel_, "");
    lv_obj_align(tempoJornadaLabel_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(tempoJornadaLabel_, theme->getTextSecondary(), LV_PART_MAIN);
    lv_obj_set_style_text_font(tempoJornadaLabel_, &lv_font_montserrat_12, LV_PART_MAIN);

    // ---- Mensagem ----
    mensagemLabel_ = lv_label_create(container_);
    lv_label_set_text(mensagemLabel_, "");
    lv_obj_align(mensagemLabel_, LV_ALIGN_RIGHT_MID, -46, 0);
    lv_obj_set_width(mensagemLabel_, 120);
    lv_label_set_long_mode(mensagemLabel_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(mensagemLabel_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_color(mensagemLabel_, theme->getTextMuted(), LV_PART_MAIN);
    lv_obj_set_style_text_font(mensagemLabel_, &lv_font_montserrat_20, LV_PART_MAIN);

    // ---- Botao TROCAR TELA (canto direito) ----
    swapBtn_ = lv_btn_create(container_);
    lv_obj_set_size(swapBtn_, 36, 32);
    lv_obj_align(swapBtn_, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(swapBtn_, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_radius(swapBtn_, 4, LV_PART_MAIN);
    lv_obj_add_flag(swapBtn_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(swapBtn_, swapBtnCallback, LV_EVENT_CLICKED, this);

    lv_obj_t* swapLabel = lv_label_create(swapBtn_);
    lv_label_set_text(swapLabel, LV_SYMBOL_REFRESH);
    lv_obj_center(swapLabel);
    lv_obj_set_style_text_color(swapLabel, theme->getTextPrimary(), LV_PART_MAIN);
    lv_obj_set_style_text_font(swapLabel, &lv_font_montserrat_14, LV_PART_MAIN);

    // ---- Timer de atualizacao ----
    updateTimer_ = lv_timer_create(updateTimerCallback, STATUS_BAR_UPDATE_MS, this);

    bsp_display_unlock();

    ESP_LOGI(TAG, "StatusBar criada em lv_layer_top()");
}

// ============================================================================
// DESTRUICAO
// ============================================================================

void StatusBar::destroy() {
    if (updateTimer_) {
        lv_timer_del(updateTimer_);
        updateTimer_ = nullptr;
    }

    if (container_ && bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        lv_obj_del(container_);
        container_ = nullptr;
        swapBtn_ = nullptr;
        ignicaoIndicator_ = nullptr;
        ignicaoLabel_ = nullptr;
        tempoIgnicaoLabel_ = nullptr;
        bleIcon_ = nullptr;
        tempoJornadaLabel_ = nullptr;
        mensagemLabel_ = nullptr;
        bsp_display_unlock();
    }

    ESP_LOGI(TAG, "StatusBar destruida");
}

// ============================================================================
// ATUALIZACAO
// ============================================================================

void StatusBar::update(const StatusBarData& data) {
    if (!container_) return;

    if (!bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        return;
    }

    Theme* theme = Theme::getInstance();

    // Atualizar indicador de ignicao
    if (ignicaoIndicator_ && ignicaoLabel_) {
        if (data.ignicaoOn) {
            lv_obj_set_style_bg_color(ignicaoIndicator_, theme->getColorSuccess(), LV_PART_MAIN);
            lv_label_set_text(ignicaoLabel_, "ON");
        } else {
            lv_obj_set_style_bg_color(ignicaoIndicator_, theme->getColorError(), LV_PART_MAIN);
            lv_label_set_text(ignicaoLabel_, "OFF");
        }
    }

    // Atualizar tempo de ignicao
    if (tempoIgnicaoLabel_) {
        if (data.ignicaoOn && data.tempoIgnicao > 0) {
            char buffer[48];
            char timeStr[TIME_FORMAT_MIN_BUFFER];
            time_format_ms(data.tempoIgnicao, timeStr, sizeof(timeStr));
            snprintf(buffer, sizeof(buffer), "Ignicao: %s", timeStr);
            lv_label_set_text(tempoIgnicaoLabel_, buffer);
        } else {
            lv_label_set_text(tempoIgnicaoLabel_, "");
        }
    }

    // Atualizar tempo de jornada
    if (tempoJornadaLabel_) {
        if (data.tempoJornada > 0) {
            char buffer[48];
            char timeStr[TIME_FORMAT_MIN_BUFFER];
            time_format_ms(data.tempoJornada, timeStr, sizeof(timeStr));
            snprintf(buffer, sizeof(buffer), "Jornada M1: %s", timeStr);
            lv_label_set_text(tempoJornadaLabel_, buffer);
        } else {
            lv_label_set_text(tempoJornadaLabel_, "");
        }
    }

    // Atualizar mensagem extra
    if (mensagemLabel_ && data.mensagem && strlen(data.mensagem) > 0) {
        lv_label_set_text(mensagemLabel_, data.mensagem);
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
    if (!mensagemLabel_) return;

    if (!bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        return;
    }

    lv_label_set_text(mensagemLabel_, message ? message : "");
    lv_obj_set_style_text_color(mensagemLabel_, color, LV_PART_MAIN);
    if (font) {
        lv_obj_set_style_text_font(mensagemLabel_, font, LV_PART_MAIN);
    }

    if (timeoutMs > 0 && message && strlen(message) > 0) {
        messageExpireTime_ = time_millis() + timeoutMs;
    } else {
        messageExpireTime_ = 0;
    }

    bsp_display_unlock();
}

void StatusBar::clearMessage() {
    setMessage("", lv_color_hex(THEME_TEXT_MUTED), &lv_font_montserrat_20, 0);
    messageExpireTime_ = 0;
}

// ============================================================================
// STATUS BLE
// ============================================================================

void StatusBar::setBleStatus(BleStatus status) {
    if (!bleIcon_) return;
    if (!bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) return;

    bleStatus_ = status;

    switch (status) {
        case BleStatus::DISCONNECTED:
            lv_obj_set_style_text_color(bleIcon_, lv_color_hex(0x666666), LV_PART_MAIN);
            lv_label_set_text(bleIcon_, LV_SYMBOL_BLUETOOTH);
            break;
        case BleStatus::ADVERTISING:
            lv_obj_set_style_text_color(bleIcon_, lv_color_hex(0x0088FF), LV_PART_MAIN);
            lv_label_set_text(bleIcon_, LV_SYMBOL_BLUETOOTH);
            break;
        case BleStatus::CONNECTED:
            lv_obj_set_style_text_color(bleIcon_, lv_color_hex(0x00AAFF), LV_PART_MAIN);
            lv_label_set_text(bleIcon_, LV_SYMBOL_BLUETOOTH);
            break;
        case BleStatus::SECURED:
            lv_obj_set_style_text_color(bleIcon_, lv_color_hex(0x00FF00), LV_PART_MAIN);
            lv_label_set_text(bleIcon_, LV_SYMBOL_BLUETOOTH);
            break;
    }

    bsp_display_unlock();
}

// ============================================================================
// SCREEN MANAGER REFERENCE
// ============================================================================

void StatusBar::setScreenManager(IScreenManager* mgr) {
    screenManager_ = mgr;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void StatusBar::updateTimerCallback(lv_timer_t* timer) {
    StatusBar* self = static_cast<StatusBar*>(timer->user_data);
    if (!self) return;

    // Verificar expiracao de mensagem
    if (self->messageExpireTime_ > 0 && time_millis() >= self->messageExpireTime_) {
        self->clearMessage();
    }
}

void StatusBar::swapBtnCallback(lv_event_t* e) {
    StatusBar* self = static_cast<StatusBar*>(lv_event_get_user_data(e));
    if (!self || !self->screenManager_) {
        ESP_LOGW(TAG, "Swap btn: sem screen manager configurado");
        return;
    }

    // Verifica se navegacao esta bloqueada (OTA em progresso)
    if (self->screenManager_->isNavigationLocked()) {
        ESP_LOGW(TAG, "Swap btn: navegacao bloqueada (OTA em progresso)");
        return;
    }

    // Ciclo de 3 telas: NUMPAD -> JORNADA -> SETTINGS -> NUMPAD
    ScreenType current = self->screenManager_->getCurrentScreen();

    ScreenType next;
    if (current == ScreenType::NUMPAD) {
        next = ScreenType::JORNADA;
    } else if (current == ScreenType::JORNADA) {
        next = ScreenType::SETTINGS;
    } else if (current == ScreenType::SETTINGS) {
        next = ScreenType::NUMPAD;
    } else {
        next = ScreenType::NUMPAD;  // fallback
    }

    ESP_LOGI(TAG, "Swap btn: trocando de %d para %d",
             static_cast<int>(current), static_cast<int>(next));
    self->screenManager_->cycleTo(next);
}
