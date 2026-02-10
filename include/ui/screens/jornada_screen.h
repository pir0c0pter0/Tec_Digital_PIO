/**
 * ============================================================================
 * TELA DE JORNADA - HEADER
 * ============================================================================
 *
 * Implementacao de IScreen que encapsula o teclado de jornada (12 botoes
 * de acao em grade 4x3) com gerenciamento de motoristas e audio feedback.
 *
 * Delega toda a logica de dominio (estado dos motoristas, popups, audio)
 * para JornadaKeyboard. Gerencia apenas os objetos LVGL da tela.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef UI_SCREENS_JORNADA_SCREEN_H
#define UI_SCREENS_JORNADA_SCREEN_H

#include "interfaces/i_screen.h"
#include "config/app_config.h"
#include "lvgl.h"

// Forward declarations
class JornadaKeyboard;
class ButtonManager;

/**
 * Tela de jornada: grade 4x3 de botoes de acao para motoristas.
 *
 * Encapsula a logica de UI do teclado de jornada, delegando
 * processamento de acoes, popups e audio ao JornadaKeyboard existente.
 * Gerencia seu proprio lv_obj_t* screen_ para integracao com ScreenManagerImpl.
 */
class JornadaScreen : public IScreen {
public:
    JornadaScreen();
    ~JornadaScreen() override;

    // IScreen interface
    ScreenType getType() const override;
    void create() override;
    void destroy() override;
    bool isCreated() const override;
    void update() override;
    void onEnter() override;
    void onExit() override;
    lv_obj_t* getLvScreen() const override;
    void invalidate() override;

private:
    // Tela LVGL
    lv_obj_t* screen_;
    bool created_;

    // Delegacao de logica de dominio
    JornadaKeyboard* jornadaKb_;
    ButtonManager* btnManager_;

    // Metodos internos
    void createButtonGrid();
    void setupGridContainer();
};

#endif // UI_SCREENS_JORNADA_SCREEN_H
