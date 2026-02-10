/**
 * ============================================================================
 * TELA DE JORNADA - IMPLEMENTACAO
 * ============================================================================
 *
 * Encapsula o teclado de jornada como IScreen para integracao com
 * ScreenManagerImpl. Delega logica de dominio ao JornadaKeyboard.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "ui/screens/jornada_screen.h"
#include "jornada_keyboard.h"
#include "button_manager.h"
#include "esp_bsp.h"
#include "esp_log.h"

static const char* TAG = "JORNADA_SCR";

// ============================================================================
// CONSTRUTOR E DESTRUTOR
// ============================================================================

JornadaScreen::JornadaScreen()
    : screen_(nullptr)
    , created_(false)
    , jornadaKb_(nullptr)
    , btnManager_(nullptr)
{
}

JornadaScreen::~JornadaScreen() {
    destroy();
}

// ============================================================================
// ISCREEN INTERFACE
// ============================================================================

ScreenType JornadaScreen::getType() const {
    return ScreenType::JORNADA;
}

void JornadaScreen::create() {
    if (created_) {
        ESP_LOGW(TAG, "JornadaScreen ja criada, ignorando create()");
        return;
    }

    ESP_LOGI(TAG, "Criando JornadaScreen...");

    // Cria ButtonManager proprio desta tela (isolamento total)
    btnManager_ = new ButtonManager();
    // Instancia per-screen (sem singleton) para isolamento total
    jornadaKb_ = new JornadaKeyboard();

    if (!btnManager_ || !jornadaKb_) {
        ESP_LOGE(TAG, "Falha ao criar ButtonManager ou JornadaKeyboard");
        return;
    }

    // Inicializa ButtonManager (cria screen e gridContainer internos)
    btnManager_->init();

    // Inicializa JornadaKeyboard com ButtonManager desta tela (nao o singleton)
    jornadaKb_->init(btnManager_);

    // Cria os 12 botoes de jornada via JornadaKeyboard
    jornadaKb_->createKeyboard();

    // Obtem o screen LVGL diretamente do ButtonManager
    screen_ = btnManager_->getScreen();

    created_ = true;
    ESP_LOGI(TAG, "JornadaScreen criada com sucesso");
}

void JornadaScreen::destroy() {
    if (!created_) {
        return;
    }

    ESP_LOGI(TAG, "Destruindo JornadaScreen...");

    // Limpa e deleta JornadaKeyboard ANTES do ButtonManager
    // (JornadaKeyboard pode referenciar objetos do ButtonManager)
    if (jornadaKb_) {
        jornadaKb_->clearKeyboard();
        delete jornadaKb_;
        jornadaKb_ = nullptr;
    }

    // Deleta o ButtonManager proprio desta tela (deleta screen LVGL e botoes)
    if (btnManager_) {
        delete btnManager_;
        btnManager_ = nullptr;
    }
    screen_ = nullptr;

    created_ = false;
    ESP_LOGI(TAG, "JornadaScreen destruida");
}

bool JornadaScreen::isCreated() const {
    return created_;
}

void JornadaScreen::update() {
    // Atualizacao periodica -- os indicadores sao gerenciados internamente
    // pelo JornadaKeyboard via timers LVGL, nao precisa de acao aqui
}

void JornadaScreen::onEnter() {
    ESP_LOGI(TAG, "JornadaScreen: onEnter");

    // Se a tela ja existe mas os botoes foram limpos, recriar
    if (jornadaKb_ && btnManager_) {
        // Verifica se os botoes existem checando o primeiro botao
        // Se nao existem, recria o teclado
        // (Pode acontecer se destroy parcial ou recreacao)
    }
}

void JornadaScreen::onExit() {
    ESP_LOGI(TAG, "JornadaScreen: onExit");

    // Fechar popup de selecao de motorista (se aberto)
    if (jornadaKb_) {
        jornadaKb_->closeMotoristaSelection();
    }

    // Fechar popup generico do ButtonManager (se aberto)
    if (btnManager_) {
        btnManager_->closePopup();
    }
}

lv_obj_t* JornadaScreen::getLvScreen() const {
    return screen_;
}

// ============================================================================
// METODOS INTERNOS
// ============================================================================

void JornadaScreen::setStatusBar(StatusBar* bar) {
    if (jornadaKb_) {
        jornadaKb_->setStatusBar(bar);
    }
}

void JornadaScreen::invalidate() {
    screen_ = nullptr;
    created_ = false;
    jornadaKb_ = nullptr;
    btnManager_ = nullptr;
    ESP_LOGI(TAG, "JornadaScreen invalidada (sem cleanup LVGL)");
}

void JornadaScreen::createButtonGrid() {
    // Delegado ao JornadaKeyboard::createKeyboard()
}

void JornadaScreen::setupGridContainer() {
    // Delegado ao ButtonManager::createScreen() via init()
}
