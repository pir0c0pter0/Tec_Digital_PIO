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

    // Cria ButtonManager proprio desta tela (isolamento total)
    btnManager_ = new ButtonManager();
    numpad_ = NumpadExample::getInstance();

    if (!btnManager_ || !numpad_) {
        ESP_LOGE(TAG, "Falha ao criar ButtonManager ou obter NumpadExample");
        return;
    }

    // Inicializa ButtonManager (cria screen e gridContainer internos)
    btnManager_->init();

    // Inicializa NumpadExample com ButtonManager desta tela (nao o singleton)
    numpad_->init(btnManager_);

    // Cria os 12 botoes do numpad via NumpadExample
    numpad_->createNumpad();

    // Obtem o screen LVGL diretamente do ButtonManager
    screen_ = btnManager_->getScreen();

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

    // Deleta o ButtonManager proprio desta tela (deleta screen LVGL e botoes)
    if (btnManager_) {
        delete btnManager_;
        btnManager_ = nullptr;
    }
    screen_ = nullptr;

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

    // Parar timer de timeout (evita callbacks em objetos deletados)
    if (numpad_) {
        numpad_->stopTimeoutTimer();
    }

    // Fechar popup generico do ButtonManager (se aberto)
    if (btnManager_) {
        btnManager_->closePopup();
    }
}

lv_obj_t* NumpadScreen::getLvScreen() const {
    return screen_;
}

// ============================================================================
// METODOS INTERNOS
// ============================================================================

void NumpadScreen::invalidate() {
    screen_ = nullptr;
    created_ = false;
    numpad_ = nullptr;
    // Nao deletar btnManager_ aqui -- invalidate assume que LVGL ja limpou
    btnManager_ = nullptr;
    ESP_LOGI(TAG, "NumpadScreen invalidada (sem cleanup LVGL)");
}

void NumpadScreen::createNumpadGrid() {
    // Delegado ao NumpadExample::createNumpad()
}

void NumpadScreen::setupGridContainer() {
    // Delegado ao ButtonManager::createScreen() via init()
}
