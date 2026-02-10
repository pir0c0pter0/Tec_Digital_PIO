/**
 * ============================================================================
 * TELA DE OTA - IMPLEMENTACAO
 * ============================================================================
 *
 * Tela de atualizacao de firmware com barra de progresso, percentual,
 * bytes transferidos e estado atual. Sem elementos interativos --
 * UI travada durante OTA (OTA-08).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "ui/screens/ota_screen.h"
#include "esp_log.h"
#include <cstdio>

static const char* TAG = "OTA_SCR";

// ============================================================================
// LAYOUT CONSTANTS
// ============================================================================

static const lv_coord_t CONTENT_Y_OFFSET = STATUS_BAR_HEIGHT;
static const lv_coord_t TITLE_Y          = CONTENT_Y_OFFSET + 20;
static const lv_coord_t STATE_Y          = CONTENT_Y_OFFSET + 60;
static const lv_coord_t PERCENT_Y        = CONTENT_Y_OFFSET + 100;
static const lv_coord_t BAR_Y            = CONTENT_Y_OFFSET + 125;
static const lv_coord_t BYTES_Y          = CONTENT_Y_OFFSET + 160;
static const lv_coord_t ERROR_Y          = CONTENT_Y_OFFSET + 200;
static const lv_coord_t BAR_WIDTH        = 440;
static const lv_coord_t BAR_HEIGHT       = 20;

// ============================================================================
// CONSTRUTOR E DESTRUTOR
// ============================================================================

OtaScreen::OtaScreen()
    : screen_(nullptr)
    , created_(false)
    , titleLabel_(nullptr)
    , stateLabel_(nullptr)
    , progressBar_(nullptr)
    , percentLabel_(nullptr)
    , bytesLabel_(nullptr)
    , errorLabel_(nullptr)
{
}

OtaScreen::~OtaScreen() {
    destroy();
}

// ============================================================================
// ISCREEN INTERFACE
// ============================================================================

ScreenType OtaScreen::getType() const {
    return ScreenType::OTA;
}

void OtaScreen::create() {
    if (created_) {
        ESP_LOGW(TAG, "OtaScreen ja criada, ignorando create()");
        return;
    }

    ESP_LOGI(TAG, "Criando OtaScreen...");

    // Cria tela LVGL
    screen_ = lv_obj_create(NULL);
    if (!screen_) {
        ESP_LOGE(TAG, "Falha ao criar tela LVGL");
        return;
    }
    lv_obj_set_style_bg_color(screen_, lv_color_hex(THEME_BG_PRIMARY), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);

    // ========================================================================
    // TITULO
    // ========================================================================

    titleLabel_ = lv_label_create(screen_);
    lv_label_set_text(titleLabel_, "Atualizacao de Firmware");
    lv_obj_set_style_text_color(titleLabel_, lv_color_hex(THEME_TEXT_PRIMARY), 0);
    lv_obj_set_width(titleLabel_, DISPLAY_WIDTH);
    lv_obj_set_style_text_align(titleLabel_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(titleLabel_, 0, TITLE_Y);

    // ========================================================================
    // LABEL DE ESTADO
    // ========================================================================

    stateLabel_ = lv_label_create(screen_);
    lv_label_set_text(stateLabel_, "Aguardando...");
    lv_obj_set_style_text_color(stateLabel_, lv_color_hex(THEME_TEXT_SECONDARY), 0);
    lv_obj_set_width(stateLabel_, DISPLAY_WIDTH);
    lv_obj_set_style_text_align(stateLabel_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(stateLabel_, 0, STATE_Y);

    // ========================================================================
    // LABEL DE PERCENTUAL
    // ========================================================================

    percentLabel_ = lv_label_create(screen_);
    lv_label_set_text(percentLabel_, "0%");
    lv_obj_set_style_text_color(percentLabel_, lv_color_hex(THEME_TEXT_PRIMARY), 0);
    lv_obj_set_width(percentLabel_, DISPLAY_WIDTH);
    lv_obj_set_style_text_align(percentLabel_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(percentLabel_, 0, PERCENT_Y);

    // ========================================================================
    // BARRA DE PROGRESSO
    // ========================================================================

    progressBar_ = lv_bar_create(screen_);
    lv_bar_set_range(progressBar_, 0, 100);
    lv_bar_set_value(progressBar_, 0, LV_ANIM_OFF);
    lv_obj_set_size(progressBar_, BAR_WIDTH, BAR_HEIGHT);
    lv_obj_set_pos(progressBar_, (DISPLAY_WIDTH - BAR_WIDTH) / 2, BAR_Y);

    // Estilo da barra
    lv_obj_set_style_bg_color(progressBar_, lv_color_hex(THEME_BG_TERTIARY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar_, lv_color_hex(THEME_COLOR_INFO), LV_PART_INDICATOR);
    lv_obj_set_style_radius(progressBar_, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(progressBar_, 4, LV_PART_INDICATOR);

    // ========================================================================
    // LABEL DE BYTES
    // ========================================================================

    bytesLabel_ = lv_label_create(screen_);
    lv_label_set_text(bytesLabel_, "0 / 0 KB");
    lv_obj_set_style_text_color(bytesLabel_, lv_color_hex(THEME_TEXT_MUTED), 0);
    lv_obj_set_width(bytesLabel_, DISPLAY_WIDTH);
    lv_obj_set_style_text_align(bytesLabel_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(bytesLabel_, 0, BYTES_Y);

    // ========================================================================
    // LABEL DE ERRO (oculto por padrao)
    // ========================================================================

    errorLabel_ = lv_label_create(screen_);
    lv_label_set_text(errorLabel_, "");
    lv_obj_set_style_text_color(errorLabel_, lv_color_hex(THEME_COLOR_ERROR), 0);
    lv_obj_set_width(errorLabel_, DISPLAY_WIDTH - 40);
    lv_obj_set_style_text_align(errorLabel_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(errorLabel_, 20, ERROR_Y);
    lv_obj_add_flag(errorLabel_, LV_OBJ_FLAG_HIDDEN);

    created_ = true;
    ESP_LOGI(TAG, "OtaScreen criada com sucesso");
}

void OtaScreen::destroy() {
    if (!created_) {
        return;
    }

    ESP_LOGI(TAG, "Destruindo OtaScreen...");

    if (screen_) {
        lv_obj_del(screen_);
        screen_ = nullptr;
    }

    // Todos os widgets filhos sao deletados automaticamente pelo LVGL
    titleLabel_ = nullptr;
    stateLabel_ = nullptr;
    progressBar_ = nullptr;
    percentLabel_ = nullptr;
    bytesLabel_ = nullptr;
    errorLabel_ = nullptr;

    created_ = false;
    ESP_LOGI(TAG, "OtaScreen destruida");
}

bool OtaScreen::isCreated() const {
    return created_;
}

void OtaScreen::update() {
    // No-op -- progresso atualizado via updateProgress() e updateState()
}

void OtaScreen::onEnter() {
    ESP_LOGI(TAG, "OtaScreen: onEnter");

    if (!created_) return;

    // Reseta UI para estado inicial
    lv_bar_set_value(progressBar_, 0, LV_ANIM_OFF);
    lv_label_set_text(percentLabel_, "0%");
    lv_label_set_text(bytesLabel_, "0 / 0 KB");
    lv_label_set_text(stateLabel_, "Aguardando...");
    lv_obj_add_flag(errorLabel_, LV_OBJ_FLAG_HIDDEN);
}

void OtaScreen::onExit() {
    ESP_LOGI(TAG, "OtaScreen: onExit");
}

lv_obj_t* OtaScreen::getLvScreen() const {
    return screen_;
}

void OtaScreen::invalidate() {
    screen_ = nullptr;
    titleLabel_ = nullptr;
    stateLabel_ = nullptr;
    progressBar_ = nullptr;
    percentLabel_ = nullptr;
    bytesLabel_ = nullptr;
    errorLabel_ = nullptr;
    created_ = false;
    ESP_LOGI(TAG, "OtaScreen invalidada (sem cleanup LVGL)");
}

// ============================================================================
// ATUALIZACAO DE PROGRESSO
// ============================================================================

void OtaScreen::updateProgress(uint8_t percent, uint32_t received, uint32_t total) {
    if (!created_) return;

    // Atualiza barra de progresso
    lv_bar_set_value(progressBar_, percent, LV_ANIM_ON);

    // Atualiza percentual
    char percentBuf[8];
    snprintf(percentBuf, sizeof(percentBuf), "%d%%", percent);
    lv_label_set_text(percentLabel_, percentBuf);

    // Atualiza bytes (em KB)
    char bytesBuf[32];
    snprintf(bytesBuf, sizeof(bytesBuf), "%lu / %lu KB",
             (unsigned long)(received / 1024),
             (unsigned long)(total / 1024));
    lv_label_set_text(bytesLabel_, bytesBuf);
}

// ============================================================================
// ATUALIZACAO DE ESTADO
// ============================================================================

void OtaScreen::updateState(OtaState state) {
    if (!created_) return;

    lv_label_set_text(stateLabel_, stateToString(state));
}

const char* OtaScreen::stateToString(OtaState state) {
    switch (state) {
        case OTA_STATE_IDLE:            return "Aguardando...";
        case OTA_STATE_PROVISIONING:    return "Provisionando...";
        case OTA_STATE_CONNECTING_WIFI: return "Conectando ao Wi-Fi...";
        case OTA_STATE_WIFI_CONNECTED:  return "Wi-Fi conectado!";
        case OTA_STATE_DISABLING_BLE:   return "Desligando Bluetooth...";
        case OTA_STATE_STARTING_HTTP:   return "Iniciando servidor...";
        case OTA_STATE_WAITING_FIRMWARE:return "Aguardando firmware...";
        case OTA_STATE_RECEIVING:       return "Recebendo firmware...";
        case OTA_STATE_VERIFYING:       return "Verificando integridade...";
        case OTA_STATE_REBOOTING:       return "Reiniciando...";
        case OTA_STATE_ABORTING:        return "Cancelando...";
        case OTA_STATE_FAILED:          return "Falha na atualizacao";
        default:                        return "Estado desconhecido";
    }
}

// ============================================================================
// EXIBICAO DE ERRO
// ============================================================================

void OtaScreen::showError(const char* message) {
    if (!created_ || !message) return;

    lv_label_set_text(errorLabel_, message);
    lv_obj_clear_flag(errorLabel_, LV_OBJ_FLAG_HIDDEN);
}
