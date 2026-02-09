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

    // Obter instancias dos singletons
    btnManager_ = ButtonManager::getInstance();
    jornadaKb_ = JornadaKeyboard::getInstance();

    if (!btnManager_ || !jornadaKb_) {
        ESP_LOGE(TAG, "Falha ao obter ButtonManager ou JornadaKeyboard");
        return;
    }

    // Inicializa ButtonManager (cria screen e gridContainer internos)
    btnManager_->init();

    // Inicializa JornadaKeyboard (conecta ao ButtonManager)
    jornadaKb_->init();

    // Cria os 12 botoes de jornada via JornadaKeyboard
    jornadaKb_->createKeyboard();

    // Obtem o screen LVGL do ButtonManager para uso como nosso screen_
    // O ButtonManager cria seu proprio screen em init()->createScreen()
    // Precisamos acessar via lv_scr_act() apos a criacao
    if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        screen_ = lv_scr_act();
        bsp_display_unlock();
    }

    created_ = true;
    ESP_LOGI(TAG, "JornadaScreen criada com sucesso");
}

void JornadaScreen::destroy() {
    if (!created_) {
        return;
    }

    ESP_LOGI(TAG, "Destruindo JornadaScreen...");

    // Limpa o teclado de jornada (para animacoes, remove botoes)
    if (jornadaKb_) {
        jornadaKb_->clearKeyboard();
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
    // Nada especial a fazer -- animacoes continuam ate destroy()
}

lv_obj_t* JornadaScreen::getLvScreen() const {
    return screen_;
}

// ============================================================================
// METODOS INTERNOS
// ============================================================================

void JornadaScreen::createButtonGrid() {
    // Delegado ao JornadaKeyboard::createKeyboard()
}

void JornadaScreen::setupGridContainer() {
    // Delegado ao ButtonManager::createScreen() via init()
}
