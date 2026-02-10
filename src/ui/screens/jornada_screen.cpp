/**
 * ============================================================================
 * TELA DE JORNADA - IMPLEMENTACAO
 * ============================================================================
 *
 * Encapsula o teclado de jornada como IScreen para integracao com
 * ScreenManagerImpl. Delega logica de dominio ao JornadaKeyboard.
 * Integra com NvsManager para persistencia de estado entre reboots.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "ui/screens/jornada_screen.h"
#include "jornada_keyboard.h"
#include "button_manager.h"
#include "services/nvs/nvs_manager.h"
#include "esp_bsp.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "JORNADA_SCR";

// ============================================================================
// STRUCT COMPACTA PARA PERSISTENCIA DO ESTADO DO TECLADO POR MOTORISTA
// ============================================================================

/**
 * Cada motorista pode estar logado em multiplas acoes simultaneamente.
 * Salvamos um bitmap de 16 bits (12 acoes usadas) por motorista.
 * Total: 3 bytes por motorista, 9 bytes para 3 motoristas.
 */
struct __attribute__((packed)) NvsKbMotoristaState {
    uint8_t version;          // NVS_JORNADA_VERSION para migracao
    uint16_t acoesBitmap;     // Bit i = motorista logado na acao i
};

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

    // Registra callback para auto-save em NVS a cada mudanca de estado
    JornadaKeyboard* kb = jornadaKb_;
    jornadaKb_->setStateChangeCallback([kb, this]() {
        saveStateToNvs();
    });

    // Restaura estado persistido do NVS (re-login motoristas nas acoes salvas)
    restoreStateFromNvs();

    created_ = true;
    ESP_LOGI(TAG, "JornadaScreen criada com sucesso");
}

void JornadaScreen::destroy() {
    if (!created_) {
        return;
    }

    ESP_LOGI(TAG, "Destruindo JornadaScreen...");

    // Salva estado antes de destruir (safety net)
    saveStateToNvs();

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
}

void JornadaScreen::onExit() {
    ESP_LOGI(TAG, "JornadaScreen: onExit");

    // Salva estado ao sair da tela (safety net em caso de perda de energia futura)
    saveStateToNvs();

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

// ============================================================================
// PERSISTENCIA NVS
// ============================================================================

void JornadaScreen::saveStateToNvs() {
    if (!jornadaKb_) return;

    NvsManager* nvs = NvsManager::getInstance();

    for (uint8_t mot = 0; mot < MAX_MOTORISTAS; mot++) {
        NvsKbMotoristaState state;
        state.version = NVS_JORNADA_VERSION;
        state.acoesBitmap = 0;

        // Monta bitmap: bit i = motorista logado na acao i
        for (int acao = 0; acao < ACAO_MAX; acao++) {
            if (jornadaKb_->isMotoristaLogado(static_cast<TipoAcao>(acao), mot)) {
                state.acoesBitmap |= (1 << acao);
            }
        }

        nvs->saveJornadaState(mot, &state, sizeof(state));
    }

    ESP_LOGD(TAG, "Estado do teclado salvo no NVS");
}

void JornadaScreen::restoreStateFromNvs() {
    if (!jornadaKb_) return;

    NvsManager* nvs = NvsManager::getInstance();
    bool restoredAny = false;

    for (uint8_t mot = 0; mot < MAX_MOTORISTAS; mot++) {
        NvsKbMotoristaState state;
        if (!nvs->loadJornadaState(mot, &state, sizeof(state))) {
            continue;  // Nao encontrado ou versao incompativel
        }

        // Re-login motorista nas acoes salvas
        for (int acao = 0; acao < ACAO_MAX; acao++) {
            if (state.acoesBitmap & (1 << acao)) {
                jornadaKb_->logarMotorista(static_cast<TipoAcao>(acao), mot);
                ESP_LOGI(TAG, "Motorista %d restaurado na acao %d", mot + 1, acao);
                restoredAny = true;
            }
        }
    }

    if (restoredAny) {
        ESP_LOGI(TAG, "Estado do teclado restaurado do NVS");
    } else {
        ESP_LOGI(TAG, "Nenhum estado salvo encontrado no NVS");
    }
}
