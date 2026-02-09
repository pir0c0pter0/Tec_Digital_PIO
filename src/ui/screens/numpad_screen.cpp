/**
 * ============================================================================
 * TELA DO NUMPAD - IMPLEMENTACAO
 * ============================================================================
 *
 * Encapsula o teclado numerico como IScreen para integracao com
 * ScreenManagerImpl. Delega logica de dominio ao NumpadExample.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "ui/screens/numpad_screen.h"
#include "numpad_example.h"
#include "button_manager.h"
#include "esp_bsp.h"
#include "esp_log.h"

static const char* TAG = "NUMPAD_SCR";

// ============================================================================
// CONSTRUTOR E DESTRUTOR
// ============================================================================

NumpadScreen::NumpadScreen()
    : screen_(nullptr)
    , created_(false)
    , numpad_(nullptr)
    , btnManager_(nullptr)
{
}

NumpadScreen::~NumpadScreen() {
    destroy();
}

// ============================================================================
// ISCREEN INTERFACE
// ============================================================================

ScreenType NumpadScreen::getType() const {
    return ScreenType::NUMPAD;
}

void NumpadScreen::create() {
    if (created_) {
        ESP_LOGW(TAG, "NumpadScreen ja criada, ignorando create()");
        return;
    }

    ESP_LOGI(TAG, "Criando NumpadScreen...");

    // Obter instancias dos singletons
    btnManager_ = ButtonManager::getInstance();
    numpad_ = NumpadExample::getInstance();

    if (!btnManager_ || !numpad_) {
        ESP_LOGE(TAG, "Falha ao obter ButtonManager ou NumpadExample");
        return;
    }

    // Inicializa ButtonManager (cria screen e gridContainer internos)
    btnManager_->init();

    // Inicializa NumpadExample (conecta ao ButtonManager)
    numpad_->init();

    // Cria os 12 botoes do numpad via NumpadExample
    numpad_->createNumpad();

    // Obtem o screen LVGL do ButtonManager para uso como nosso screen_
    if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        screen_ = lv_scr_act();
        bsp_display_unlock();
    }

    created_ = true;
    ESP_LOGI(TAG, "NumpadScreen criada com sucesso");
}

void NumpadScreen::destroy() {
    if (!created_) {
        return;
    }

    ESP_LOGI(TAG, "Destruindo NumpadScreen...");

    // Limpa o numpad (para timeout timer, remove botoes)
    if (numpad_) {
        numpad_->clearNumpad();
    }

    // Remove todos os botoes do ButtonManager
    if (btnManager_) {
        btnManager_->removeAllButtons();
    }

    // Deleta o screen LVGL
    if (screen_ != nullptr) {
        if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
            // Nao deletar se for a tela ativa -- ScreenManager cuida da transicao
            if (screen_ != lv_scr_act()) {
                lv_obj_del(screen_);
            }
            bsp_display_unlock();
        }
        screen_ = nullptr;
    }

    created_ = false;
    ESP_LOGI(TAG, "NumpadScreen destruida");
}

bool NumpadScreen::isCreated() const {
    return created_;
}

void NumpadScreen::update() {
    // Atualizacao periodica -- o timeout e gerenciado internamente
    // pelo NumpadExample via timer LVGL, nao precisa de acao aqui
}

void NumpadScreen::onEnter() {
    ESP_LOGI(TAG, "NumpadScreen: onEnter");

    // Ao entrar, garante que o display mostra a mensagem inicial
    if (numpad_) {
        numpad_->resetToInitialMessage();
    }
}

void NumpadScreen::onExit() {
    ESP_LOGI(TAG, "NumpadScreen: onExit");
    // Nada especial -- timeout timer continua ate destroy()
}

lv_obj_t* NumpadScreen::getLvScreen() const {
    return screen_;
}

// ============================================================================
// METODOS INTERNOS
// ============================================================================

void NumpadScreen::createNumpadGrid() {
    // Delegado ao NumpadExample::createNumpad()
}

void NumpadScreen::setupGridContainer() {
    // Delegado ao ButtonManager::createScreen() via init()
}
