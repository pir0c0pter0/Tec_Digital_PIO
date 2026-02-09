/**
 * ============================================================================
 * GERENCIADOR DE TELAS - IMPLEMENTACAO
 * ============================================================================
 *
 * Navegacao baseada em pilha com transicoes animadas.
 * Push = slide left, Pop = slide right, duracao configuravel.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "ui/screen_manager.h"
#include "ui/widgets/status_bar.h"
#include "config/app_config.h"
#include "esp_bsp.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "SCREEN_MGR";

// ============================================================================
// SINGLETON
// ============================================================================

ScreenManagerImpl* ScreenManagerImpl::instance_ = nullptr;

ScreenManagerImpl* ScreenManagerImpl::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new ScreenManagerImpl();
    }
    return instance_;
}

// ============================================================================
// CONSTRUTOR
// ============================================================================

ScreenManagerImpl::ScreenManagerImpl()
    : stackTop_(-1)
    , currentScreen_(ScreenType::SPLASH)
    , transitioning_(false)
    , transitionTimer_(nullptr)
    , statusBar_(nullptr)
{
    memset(screens_, 0, sizeof(screens_));
    memset(navStack_, 0, sizeof(navStack_));
}

// ============================================================================
// INIT
// ============================================================================

void ScreenManagerImpl::init() {
    ESP_LOGI(TAG, "Inicializando ScreenManager");

    for (int i = 0; i < static_cast<int>(ScreenType::MAX_SCREENS); i++) {
        screens_[i] = nullptr;
    }

    stackTop_ = -1;
    currentScreen_ = ScreenType::SPLASH;
    transitioning_ = false;

    if (transitionTimer_ != nullptr) {
        lv_timer_del(transitionTimer_);
        transitionTimer_ = nullptr;
    }

    ESP_LOGI(TAG, "ScreenManager inicializado (stack max: %d)", SCREEN_NAV_STACK_MAX);
}

// ============================================================================
// REGISTRAR TELA
// ============================================================================

void ScreenManagerImpl::registerScreen(IScreen* screen) {
    if (screen == nullptr) {
        ESP_LOGW(TAG, "Tentativa de registrar tela nula");
        return;
    }

    int idx = static_cast<int>(screen->getType());
    if (idx < 0 || idx >= static_cast<int>(ScreenType::MAX_SCREENS)) {
        ESP_LOGE(TAG, "Tipo de tela invalido: %d", idx);
        return;
    }

    screens_[idx] = screen;
    ESP_LOGI(TAG, "Tela registrada: tipo=%d", idx);
}

// ============================================================================
// NAVEGAR PARA TELA (PUSH)
// ============================================================================

void ScreenManagerImpl::navigateTo(ScreenType type) {
    // Guarda de transicao: bloqueia navegacao durante animacao
    if (transitioning_) {
        ESP_LOGW(TAG, "Navegacao bloqueada: transicao em andamento");
        return;
    }

    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(ScreenType::MAX_SCREENS)) {
        ESP_LOGE(TAG, "Tipo de tela invalido para navegacao: %d", idx);
        return;
    }

    IScreen* targetScreen = screens_[idx];
    if (targetScreen == nullptr) {
        ESP_LOGE(TAG, "Tela nao registrada: tipo=%d", idx);
        return;
    }

    // Verificar se a pilha esta cheia
    if (stackTop_ >= (SCREEN_NAV_STACK_MAX - 1)) {
        ESP_LOGE(TAG, "Pilha de navegacao cheia (max=%d), recusando push", SCREEN_NAV_STACK_MAX);
        return;
    }

    // Obter tela atual
    IScreen* currentScreenPtr = screens_[static_cast<int>(currentScreen_)];

    // Empurrar tela atual na pilha
    stackTop_++;
    navStack_[stackTop_] = currentScreen_;
    ESP_LOGI(TAG, "Push na pilha: tipo=%d (profundidade=%d)", static_cast<int>(currentScreen_), stackTop_ + 1);

    // Chamar onExit na tela que esta saindo
    if (currentScreenPtr != nullptr) {
        currentScreenPtr->onExit();
    }

    // Criar a nova tela se necessario
    if (!targetScreen->isCreated()) {
        targetScreen->create();
    }

    // Chamar onEnter na nova tela
    targetScreen->onEnter();

    // Animar transicao (slide left)
    lv_obj_t* newLvScreen = targetScreen->getLvScreen();
    if (newLvScreen != nullptr) {
        if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
            lv_scr_load_anim(newLvScreen,
                             LV_SCR_LOAD_ANIM_MOVE_LEFT,
                             SCREEN_TRANSITION_TIME_MS,
                             SCREEN_TRANSITION_DELAY_MS,
                             false);  // auto_del = false (gerenciamos ciclo de vida manualmente)
            bsp_display_unlock();
        }
    }

    // Atualizar tela atual
    currentScreen_ = type;

    // Definir flag de transicao e criar timer one-shot para limpar
    transitioning_ = true;
    if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        transitionTimer_ = lv_timer_create(
            transitionDoneCallback,
            SCREEN_TRANSITION_TIME_MS + 50,  // +50ms buffer para garantir que animacao terminou
            this
        );
        lv_timer_set_repeat_count(transitionTimer_, 1);  // Single-shot
        bsp_display_unlock();
    }

    // Atualizar visibilidade do botao voltar na StatusBar
    if (statusBar_ != nullptr) {
        statusBar_->setBackVisible(stackTop_ >= 0);
    }

    ESP_LOGI(TAG, "Navegou para tela tipo=%d (pilha profundidade=%d)", idx, stackTop_ + 1);
}

// ============================================================================
// VOLTAR (POP)
// ============================================================================

bool ScreenManagerImpl::goBack() {
    // Guarda de transicao
    if (transitioning_) {
        ESP_LOGW(TAG, "goBack bloqueado: transicao em andamento");
        return false;
    }

    // Verificar se a pilha esta vazia
    if (stackTop_ < 0) {
        ESP_LOGW(TAG, "Pilha de navegacao vazia, nao pode voltar");
        return false;
    }

    // Obter a tela atual (que sera destruida)
    IScreen* leavingScreen = screens_[static_cast<int>(currentScreen_)];

    // Pop: obter a tela anterior da pilha
    ScreenType previousType = navStack_[stackTop_];
    stackTop_--;

    ESP_LOGI(TAG, "Pop da pilha: voltando para tipo=%d (profundidade=%d)",
             static_cast<int>(previousType), stackTop_ + 1);

    // Chamar onExit na tela que esta saindo
    if (leavingScreen != nullptr) {
        leavingScreen->onExit();
    }

    // Obter a tela anterior
    IScreen* previousScreen = screens_[static_cast<int>(previousType)];
    if (previousScreen == nullptr) {
        ESP_LOGE(TAG, "Tela anterior nao encontrada: tipo=%d", static_cast<int>(previousType));
        return false;
    }

    // Criar a tela anterior se necessario
    if (!previousScreen->isCreated()) {
        previousScreen->create();
    }

    // Chamar onEnter na tela restaurada
    previousScreen->onEnter();

    // Animar transicao (slide right)
    lv_obj_t* prevLvScreen = previousScreen->getLvScreen();
    if (prevLvScreen != nullptr) {
        if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
            lv_scr_load_anim(prevLvScreen,
                             LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                             SCREEN_TRANSITION_TIME_MS,
                             SCREEN_TRANSITION_DELAY_MS,
                             false);  // auto_del = false
            bsp_display_unlock();
        }
    }

    // Destruir a tela que estamos saindo (liberar memoria)
    if (leavingScreen != nullptr) {
        leavingScreen->destroy();
    }

    // Atualizar tela atual
    currentScreen_ = previousType;

    // Definir flag de transicao e criar timer one-shot
    transitioning_ = true;
    if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
        transitionTimer_ = lv_timer_create(
            transitionDoneCallback,
            SCREEN_TRANSITION_TIME_MS + 50,
            this
        );
        lv_timer_set_repeat_count(transitionTimer_, 1);
        bsp_display_unlock();
    }

    // Atualizar visibilidade do botao voltar na StatusBar
    if (statusBar_ != nullptr) {
        statusBar_->setBackVisible(stackTop_ >= 0);
    }

    ESP_LOGI(TAG, "Voltou para tela tipo=%d", static_cast<int>(previousType));
    return true;
}

// ============================================================================
// TELA ATUAL
// ============================================================================

ScreenType ScreenManagerImpl::getCurrentScreen() const {
    return currentScreen_;
}

// ============================================================================
// UPDATE
// ============================================================================

void ScreenManagerImpl::update() {
    int idx = static_cast<int>(currentScreen_);
    if (idx >= 0 && idx < static_cast<int>(ScreenType::MAX_SCREENS)) {
        IScreen* screen = screens_[idx];
        if (screen != nullptr && screen->isCreated()) {
            screen->update();
        }
    }
}

// ============================================================================
// SET STATUS BAR
// ============================================================================

void ScreenManagerImpl::setStatusBar(StatusBar* bar) {
    statusBar_ = bar;
}

// ============================================================================
// TELA INICIAL (SEM ANIMACAO)
// ============================================================================

void ScreenManagerImpl::showInitialScreen(ScreenType type) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(ScreenType::MAX_SCREENS)) {
        ESP_LOGE(TAG, "Tipo de tela invalido para tela inicial: %d", idx);
        return;
    }

    IScreen* screen = screens_[idx];
    if (screen == nullptr) {
        ESP_LOGE(TAG, "Tela inicial nao registrada: tipo=%d", idx);
        return;
    }

    // Criar se necessario
    if (!screen->isCreated()) {
        screen->create();
    }

    // Carregar sem animacao
    lv_obj_t* lvScreen = screen->getLvScreen();
    if (lvScreen != nullptr) {
        if (bsp_display_lock(DISPLAY_LOCK_TIMEOUT)) {
            lv_scr_load(lvScreen);
            bsp_display_unlock();
        }
    }

    // Chamar onEnter
    screen->onEnter();

    // Definir como tela atual (sem push na pilha)
    currentScreen_ = type;

    // Botao voltar escondido (pilha vazia)
    if (statusBar_ != nullptr) {
        statusBar_->setBackVisible(false);
    }

    ESP_LOGI(TAG, "Tela inicial carregada: tipo=%d", idx);
}

// ============================================================================
// CALLBACK DE FIM DE TRANSICAO
// ============================================================================

void ScreenManagerImpl::transitionDoneCallback(lv_timer_t* timer) {
    ScreenManagerImpl* self = static_cast<ScreenManagerImpl*>(timer->user_data);
    if (self != nullptr) {
        self->transitioning_ = false;
        self->transitionTimer_ = nullptr;
        ESP_LOGD("SCREEN_MGR", "Transicao concluida");
    }
}

// ============================================================================
// INTERFACE C (WRAPPERS)
// ============================================================================

extern "C" {

void screen_manager_init(void) {
    ScreenManagerImpl::getInstance()->init();
}

void screen_navigate_to(screen_type_t type) {
    ScreenManagerImpl::getInstance()->navigateTo(static_cast<ScreenType>(type));
}

screen_type_t screen_get_current(void) {
    return static_cast<screen_type_t>(ScreenManagerImpl::getInstance()->getCurrentScreen());
}

void screen_go_back(void) {
    ScreenManagerImpl::getInstance()->goBack();
}

}  // extern "C"
