/**
 * ============================================================================
 * TELA DE CONFIGURACOES - IMPLEMENTACAO
 * ============================================================================
 *
 * Tela de configuracoes com sliders de volume e brilho (efeito imediato
 * no hardware + persistencia NVS), informacoes do sistema e botao voltar.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "ui/screens/settings_screen.h"
#include "services/nvs/nvs_manager.h"
#include "simple_audio_manager.h"
#include "display.h"
#include "esp_bsp.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "utils/debug_utils.h"
#include <cstdio>

static const char* TAG = "SETTINGS_SCR";

// ============================================================================
// LAYOUT CONSTANTS
// ============================================================================

// Area de conteudo: comeca apos status bar (40px), total 280px de altura
static const lv_coord_t CONTENT_Y_OFFSET = STATUS_BAR_HEIGHT;
static const lv_coord_t LABEL_X          = 20;
static const lv_coord_t SLIDER_X         = 140;
static const lv_coord_t SLIDER_WIDTH     = 200;
static const lv_coord_t VALUE_X          = 360;
static const lv_coord_t SECTION_HEIGHT   = 50;

// Y positions relativas ao conteudo
static const lv_coord_t VOLUME_Y         = CONTENT_Y_OFFSET + 15;
static const lv_coord_t BRIGHTNESS_Y     = CONTENT_Y_OFFSET + 75;
static const lv_coord_t SEPARATOR_Y      = CONTENT_Y_OFFSET + 130;
static const lv_coord_t INFO_START_Y     = CONTENT_Y_OFFSET + 145;
static const lv_coord_t BACK_BTN_Y       = CONTENT_Y_OFFSET + 240;

// Intervalo de refresh de info do sistema (ms)
static const uint32_t INFO_REFRESH_MS    = 1000;

// ============================================================================
// CONSTRUTOR E DESTRUTOR
// ============================================================================

SettingsScreen::SettingsScreen()
    : screen_(nullptr)
    , created_(false)
    , volumeSlider_(nullptr)
    , volumeLabel_(nullptr)
    , brightnessSlider_(nullptr)
    , brightnessLabel_(nullptr)
    , fwVersionLabel_(nullptr)
    , uptimeLabel_(nullptr)
    , memoryLabel_(nullptr)
    , backBtn_(nullptr)
    , updatingFromExternal_(false)
    , lastInfoUpdate_(0)
{
}

SettingsScreen::~SettingsScreen() {
    destroy();
}

// ============================================================================
// ISCREEN INTERFACE
// ============================================================================

ScreenType SettingsScreen::getType() const {
    return ScreenType::SETTINGS;
}

void SettingsScreen::create() {
    if (created_) {
        ESP_LOGW(TAG, "SettingsScreen ja criada, ignorando create()");
        return;
    }

    ESP_LOGI(TAG, "Criando SettingsScreen...");

    // Cria tela LVGL
    screen_ = lv_obj_create(NULL);
    if (!screen_) {
        ESP_LOGE(TAG, "Falha ao criar tela LVGL");
        return;
    }
    lv_obj_set_style_bg_color(screen_, lv_color_hex(THEME_BG_PRIMARY), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);

    // ========================================================================
    // SECAO DE VOLUME
    // ========================================================================

    // Label "Volume"
    lv_obj_t* volTitleLabel = lv_label_create(screen_);
    lv_label_set_text(volTitleLabel, "Volume");
    lv_obj_set_style_text_color(volTitleLabel, lv_color_hex(THEME_TEXT_PRIMARY), 0);
    lv_obj_set_pos(volTitleLabel, LABEL_X, VOLUME_Y);

    // Slider de volume
    volumeSlider_ = lv_slider_create(screen_);
    lv_slider_set_range(volumeSlider_, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);
    lv_obj_set_width(volumeSlider_, SLIDER_WIDTH);
    lv_obj_set_pos(volumeSlider_, SLIDER_X, VOLUME_Y);
    lv_obj_add_event_cb(volumeSlider_, onVolumeChanged, LV_EVENT_VALUE_CHANGED, this);

    // Estilo do slider
    lv_obj_set_style_bg_color(volumeSlider_, lv_color_hex(THEME_BG_TERTIARY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(volumeSlider_, lv_color_hex(THEME_COLOR_INFO), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volumeSlider_, lv_color_hex(THEME_TEXT_PRIMARY), LV_PART_KNOB);

    // Label de valor do volume
    volumeLabel_ = lv_label_create(screen_);
    lv_label_set_text(volumeLabel_, "0");
    lv_obj_set_style_text_color(volumeLabel_, lv_color_hex(THEME_TEXT_SECONDARY), 0);
    lv_obj_set_pos(volumeLabel_, VALUE_X, VOLUME_Y);

    // ========================================================================
    // SECAO DE BRILHO
    // ========================================================================

    // Label "Brilho"
    lv_obj_t* brTitleLabel = lv_label_create(screen_);
    lv_label_set_text(brTitleLabel, "Brilho");
    lv_obj_set_style_text_color(brTitleLabel, lv_color_hex(THEME_TEXT_PRIMARY), 0);
    lv_obj_set_pos(brTitleLabel, LABEL_X, BRIGHTNESS_Y);

    // Slider de brilho
    brightnessSlider_ = lv_slider_create(screen_);
    lv_slider_set_range(brightnessSlider_, 0, 100);
    lv_obj_set_width(brightnessSlider_, SLIDER_WIDTH);
    lv_obj_set_pos(brightnessSlider_, SLIDER_X, BRIGHTNESS_Y);
    lv_obj_add_event_cb(brightnessSlider_, onBrightnessChanged, LV_EVENT_VALUE_CHANGED, this);

    // Estilo do slider
    lv_obj_set_style_bg_color(brightnessSlider_, lv_color_hex(THEME_BG_TERTIARY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightnessSlider_, lv_color_hex(THEME_COLOR_WARNING), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightnessSlider_, lv_color_hex(THEME_TEXT_PRIMARY), LV_PART_KNOB);

    // Label de valor do brilho
    brightnessLabel_ = lv_label_create(screen_);
    lv_label_set_text(brightnessLabel_, "0%");
    lv_obj_set_style_text_color(brightnessLabel_, lv_color_hex(THEME_TEXT_SECONDARY), 0);
    lv_obj_set_pos(brightnessLabel_, VALUE_X, BRIGHTNESS_Y);

    // ========================================================================
    // SEPARADOR HORIZONTAL
    // ========================================================================

    lv_obj_t* separator = lv_obj_create(screen_);
    lv_obj_set_size(separator, DISPLAY_WIDTH - 40, 2);
    lv_obj_set_pos(separator, 20, SEPARATOR_Y);
    lv_obj_set_style_bg_color(separator, lv_color_hex(THEME_BG_TERTIARY), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(separator, 0, 0);
    lv_obj_set_style_radius(separator, 0, 0);
    lv_obj_set_style_pad_all(separator, 0, 0);

    // ========================================================================
    // INFORMACOES DO SISTEMA
    // ========================================================================

    // Firmware version
    fwVersionLabel_ = lv_label_create(screen_);
    char fwBuf[48];
    snprintf(fwBuf, sizeof(fwBuf), "Firmware: %s", APP_VERSION_STRING);
    lv_label_set_text(fwVersionLabel_, fwBuf);
    lv_obj_set_style_text_color(fwVersionLabel_, lv_color_hex(THEME_TEXT_MUTED), 0);
    lv_obj_set_pos(fwVersionLabel_, LABEL_X, INFO_START_Y);

    // Uptime
    uptimeLabel_ = lv_label_create(screen_);
    lv_label_set_text(uptimeLabel_, "Uptime: 00:00:00");
    lv_obj_set_style_text_color(uptimeLabel_, lv_color_hex(THEME_TEXT_MUTED), 0);
    lv_obj_set_pos(uptimeLabel_, LABEL_X, INFO_START_Y + 25);

    // Memoria livre
    memoryLabel_ = lv_label_create(screen_);
    lv_label_set_text(memoryLabel_, "Memoria livre: --- KB");
    lv_obj_set_style_text_color(memoryLabel_, lv_color_hex(THEME_TEXT_MUTED), 0);
    lv_obj_set_pos(memoryLabel_, LABEL_X, INFO_START_Y + 50);

    // ========================================================================
    // BOTAO VOLTAR
    // ========================================================================

    backBtn_ = lv_btn_create(screen_);
    lv_obj_set_size(backBtn_, 120, 40);
    lv_obj_set_pos(backBtn_, (DISPLAY_WIDTH - 120) / 2, BACK_BTN_Y);
    lv_obj_set_style_bg_color(backBtn_, lv_color_hex(THEME_BG_TERTIARY), 0);
    lv_obj_add_event_cb(backBtn_, onBackClicked, LV_EVENT_CLICKED, this);

    lv_obj_t* btnLabel = lv_label_create(backBtn_);
    lv_label_set_text(btnLabel, "Voltar");
    lv_obj_center(btnLabel);
    lv_obj_set_style_text_color(btnLabel, lv_color_hex(THEME_TEXT_PRIMARY), 0);

    created_ = true;
    ESP_LOGI(TAG, "SettingsScreen criada com sucesso");
}

void SettingsScreen::destroy() {
    if (!created_) {
        return;
    }

    ESP_LOGI(TAG, "Destruindo SettingsScreen...");

    if (screen_) {
        lv_obj_del(screen_);
        screen_ = nullptr;
    }

    // Todos os widgets filhos sao deletados automaticamente pelo LVGL
    volumeSlider_ = nullptr;
    volumeLabel_ = nullptr;
    brightnessSlider_ = nullptr;
    brightnessLabel_ = nullptr;
    fwVersionLabel_ = nullptr;
    uptimeLabel_ = nullptr;
    memoryLabel_ = nullptr;
    backBtn_ = nullptr;

    created_ = false;
    ESP_LOGI(TAG, "SettingsScreen destruida");
}

bool SettingsScreen::isCreated() const {
    return created_;
}

void SettingsScreen::update() {
    if (!created_) return;

    // Atualiza info do sistema apenas a cada INFO_REFRESH_MS (nao a cada 5ms)
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);  // microseconds -> milliseconds
    if ((now - lastInfoUpdate_) < INFO_REFRESH_MS) {
        return;
    }
    lastInfoUpdate_ = now;

    // Uptime em HH:MM:SS
    uint32_t uptimeSec = (uint32_t)(esp_timer_get_time() / 1000000);
    uint32_t hours   = uptimeSec / 3600;
    uint32_t minutes = (uptimeSec % 3600) / 60;
    uint32_t seconds = uptimeSec % 60;

    char uptimeBuf[32];
    snprintf(uptimeBuf, sizeof(uptimeBuf), "Uptime: %02lu:%02lu:%02lu",
             (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
    lv_label_set_text(uptimeLabel_, uptimeBuf);

    // Memoria livre em KB
    uint32_t freeKb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
    char memBuf[32];
    snprintf(memBuf, sizeof(memBuf), "Memoria livre: %lu KB", (unsigned long)freeKb);
    lv_label_set_text(memoryLabel_, memBuf);
}

void SettingsScreen::onEnter() {
    ESP_LOGI(TAG, "SettingsScreen: onEnter");

    if (!created_) return;

    // Carrega valores atuais do NVS
    NvsManager* nvs = NvsManager::getInstance();
    uint8_t currentVolume = nvs->loadVolume(AUDIO_VOLUME_DEFAULT);
    uint8_t currentBrightness = nvs->loadBrightness(100);

    // Seta sliders sem disparar callbacks de hardware
    updatingFromExternal_ = true;

    lv_slider_set_value(volumeSlider_, currentVolume, LV_ANIM_OFF);
    char volBuf[8];
    snprintf(volBuf, sizeof(volBuf), "%d", currentVolume);
    lv_label_set_text(volumeLabel_, volBuf);

    lv_slider_set_value(brightnessSlider_, currentBrightness, LV_ANIM_OFF);
    char brBuf[8];
    snprintf(brBuf, sizeof(brBuf), "%d%%", currentBrightness);
    lv_label_set_text(brightnessLabel_, brBuf);

    updatingFromExternal_ = false;

    // Forca refresh imediato de info do sistema
    lastInfoUpdate_ = 0;
    update();

    ESP_LOGI(TAG, "Sliders inicializados: volume=%d, brilho=%d%%", currentVolume, currentBrightness);
}

void SettingsScreen::onExit() {
    ESP_LOGI(TAG, "SettingsScreen: onExit");
}

lv_obj_t* SettingsScreen::getLvScreen() const {
    return screen_;
}

void SettingsScreen::invalidate() {
    screen_ = nullptr;
    volumeSlider_ = nullptr;
    volumeLabel_ = nullptr;
    brightnessSlider_ = nullptr;
    brightnessLabel_ = nullptr;
    fwVersionLabel_ = nullptr;
    uptimeLabel_ = nullptr;
    memoryLabel_ = nullptr;
    backBtn_ = nullptr;
    created_ = false;
    ESP_LOGI(TAG, "SettingsScreen invalidada (sem cleanup LVGL)");
}

// ============================================================================
// ATUALIZACAO EXTERNA (BLE SYNC)
// ============================================================================

void SettingsScreen::updateVolumeSlider(uint8_t vol) {
    if (!created_ || !volumeSlider_) return;

    updatingFromExternal_ = true;
    lv_slider_set_value(volumeSlider_, vol, LV_ANIM_OFF);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", vol);
    lv_label_set_text(volumeLabel_, buf);

    updatingFromExternal_ = false;
}

void SettingsScreen::updateBrightnessSlider(uint8_t brightness) {
    if (!created_ || !brightnessSlider_) return;

    updatingFromExternal_ = true;
    lv_slider_set_value(brightnessSlider_, brightness, LV_ANIM_OFF);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", brightness);
    lv_label_set_text(brightnessLabel_, buf);

    updatingFromExternal_ = false;
}

// ============================================================================
// CALLBACKS ESTATICOS (LVGL)
// ============================================================================

void SettingsScreen::onVolumeChanged(lv_event_t* e) {
    SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!self || self->updatingFromExternal_) return;

    int32_t value = lv_slider_get_value(self->volumeSlider_);

    // Aplica imediatamente no hardware
    setAudioVolume(value);

    // Persiste no NVS
    NvsManager::getInstance()->saveVolume((uint8_t)value);

    // Atualiza label
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", (int)value);
    lv_label_set_text(self->volumeLabel_, buf);

    ESP_LOGD(TAG, "Volume alterado: %d", (int)value);
}

void SettingsScreen::onBrightnessChanged(lv_event_t* e) {
    SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!self || self->updatingFromExternal_) return;

    int32_t value = lv_slider_get_value(self->brightnessSlider_);

    // Aplica imediatamente no hardware
    bsp_display_brightness_set(value);

    // Persiste no NVS
    NvsManager::getInstance()->saveBrightness((uint8_t)value);

    // Atualiza label
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (int)value);
    lv_label_set_text(self->brightnessLabel_, buf);

    ESP_LOGD(TAG, "Brilho alterado: %d%%", (int)value);
}

void SettingsScreen::onBackClicked(lv_event_t* e) {
    (void)e;  // user_data nao necessario para back
    ESP_LOGI(TAG, "Botao Voltar pressionado");
    screen_go_back();
}
