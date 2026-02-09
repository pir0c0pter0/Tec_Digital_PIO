/**
 * ============================================================================
 * TELA DO NUMPAD - HEADER
 * ============================================================================
 *
 * Implementacao de IScreen que encapsula o teclado numerico com entrada
 * de digitos, validacao, timeout automatico e feedback de audio.
 *
 * Delega toda a logica de input (digitacao, clear, timeout, envio)
 * para NumpadExample. Gerencia apenas os objetos LVGL da tela.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef UI_SCREENS_NUMPAD_SCREEN_H
#define UI_SCREENS_NUMPAD_SCREEN_H

#include "interfaces/i_screen.h"
#include "config/app_config.h"
#include "lvgl.h"

// Forward declarations
class NumpadExample;
class ButtonManager;

/**
 * Tela do numpad: grade 4x3 de botoes numericos com display.
 *
 * Encapsula a logica de UI do teclado numerico, delegando
 * processamento de digitos, timeout e validacao ao NumpadExample existente.
 * Gerencia seu proprio lv_obj_t* screen_ para integracao com ScreenManagerImpl.
 */
class NumpadScreen : public IScreen {
public:
    NumpadScreen();
    ~NumpadScreen() override;

    // IScreen interface
    ScreenType getType() const override;
    void create() override;
    void destroy() override;
    bool isCreated() const override;
    void update() override;
    void onEnter() override;
    void onExit() override;
    lv_obj_t* getLvScreen() const override;

private:
    // Tela LVGL
    lv_obj_t* screen_;
    bool created_;

    // Delegacao de logica de dominio
    NumpadExample* numpad_;
    ButtonManager* btnManager_;

    // Metodos internos
    void createNumpadGrid();
    void setupGridContainer();
};

#endif // UI_SCREENS_NUMPAD_SCREEN_H
