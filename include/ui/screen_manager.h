/**
 * ============================================================================
 * GERENCIADOR DE TELAS - HEADER
 * ============================================================================
 *
 * Implementacao do IScreenManager com navegacao baseada em pilha
 * e troca instantanea (sem animacao). Cada tela eh 100% isolada.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef UI_SCREEN_MANAGER_H
#define UI_SCREEN_MANAGER_H

#include "interfaces/i_screen.h"
#include "config/app_config.h"
#include "lvgl.h"

#ifdef __cplusplus

// Forward declaration
class StatusBar;

/**
 * Implementacao concreta do IScreenManager
 *
 * Gerencia uma pilha de navegacao de telas com troca instantanea.
 * Cada tela possui seu proprio ButtonManager para isolamento total.
 */
class ScreenManagerImpl : public IScreenManager {
public:
    /**
     * Obtem a instancia singleton
     * @return Ponteiro para o ScreenManagerImpl
     */
    static ScreenManagerImpl* getInstance();

    /**
     * Inicializa o gerenciador de telas
     * Limpa todos os slots e reseta a pilha de navegacao
     */
    void init() override;

    /**
     * Navega para uma tela (push na pilha)
     * A tela atual eh empurrada na pilha e a nova tela eh carregada
     * com animacao slide-left.
     * @param type Tipo da tela destino
     */
    void navigateTo(ScreenType type) override;

    /**
     * Volta para a tela anterior (pop da pilha)
     * A tela atual eh destruida e a tela do topo da pilha
     * eh carregada com animacao slide-right.
     * @return true se conseguiu voltar, false se pilha vazia
     */
    bool goBack() override;

    /**
     * Cicla para uma tela sem usar a pilha de navegacao
     * Apenas troca a tela atual pela destino com animacao.
     * Nao empurra nem desempilha nada.
     * @param type Tipo da tela destino
     */
    void cycleTo(ScreenType type) override;

    /**
     * Registra uma tela no gerenciador
     * @param screen Ponteiro para a tela (deve implementar IScreen)
     */
    void registerScreen(IScreen* screen) override;

    /**
     * Obtem o tipo da tela atualmente ativa
     * @return Tipo da tela atual
     */
    ScreenType getCurrentScreen() const override;

    /**
     * Atualiza a tela atual (chamado periodicamente)
     */
    void update() override;

    /**
     * Define a referencia para a StatusBar persistente
     * @param bar Ponteiro para a StatusBar
     */
    void setStatusBar(StatusBar* bar);

    /**
     * Carrega a tela inicial sem animacao e sem empurrar na pilha
     * Usado apenas na inicializacao do sistema (boot).
     * @param type Tipo da tela inicial
     */
    void showInitialScreen(ScreenType type);

    /**
     * Obtem a profundidade atual da pilha de navegacao
     * @return Numero de telas na pilha
     */
    int getStackDepth() const { return stackTop_ + 1; }

private:
    ScreenManagerImpl();
    ~ScreenManagerImpl() = default;

    // Singleton
    static ScreenManagerImpl* instance_;

    // Telas registradas (array indexado por ScreenType)
    IScreen* screens_[static_cast<int>(ScreenType::MAX_SCREENS)];

    // Pilha de navegacao (tamanho fixo, sem heap allocation)
    ScreenType navStack_[SCREEN_NAV_STACK_MAX];
    int stackTop_;

    // Estado
    ScreenType currentScreen_;

    // Referencia para StatusBar persistente
    StatusBar* statusBar_;
};

#endif // __cplusplus

#endif // UI_SCREEN_MANAGER_H
