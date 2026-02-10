/**
 * ============================================================================
 * TELA DE CONFIGURACOES - HEADER
 * ============================================================================
 *
 * Implementacao de IScreen para tela de configuracoes do sistema.
 * Exibe sliders de volume e brilho com efeito imediato no hardware,
 * informacoes do sistema (versao, uptime, memoria) e botao de voltar.
 *
 * Nao usa ButtonManager -- cria widgets LVGL diretamente na tela.
 * Metodos publicos updateVolumeSlider/updateBrightnessSlider permitem
 * sincronizacao externa (BLE) sem feedback loop.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef UI_SCREENS_SETTINGS_SCREEN_H
#define UI_SCREENS_SETTINGS_SCREEN_H

#include "interfaces/i_screen.h"
#include "config/app_config.h"
#include "lvgl.h"

// Forward declarations
class IScreenManager;

/**
 * Tela de configuracoes: sliders de volume/brilho + info do sistema.
 *
 * Layout (480x320, 40px status bar):
 * - Volume slider (0-21) com label de valor
 * - Brilho slider (0-100%) com label de valor
 * - Separador horizontal
 * - Info: firmware, uptime, memoria livre
 * - Botao "Voltar" no centro inferior
 */
class SettingsScreen : public IScreen {
public:
    SettingsScreen();
    ~SettingsScreen() override;

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

    /**
     * Atualiza slider de volume externamente (ex: via BLE sync).
     * Nao dispara callback de hardware/NVS (usa updatingFromExternal_).
     * @param vol Volume de 0 a AUDIO_VOLUME_MAX
     */
    void updateVolumeSlider(uint8_t vol);

    /**
     * Atualiza slider de brilho externamente (ex: via BLE sync).
     * Nao dispara callback de hardware/NVS (usa updatingFromExternal_).
     * @param brightness Brilho de 0 a 100
     */
    void updateBrightnessSlider(uint8_t brightness);

private:
    // Tela LVGL
    lv_obj_t* screen_;
    bool created_;

    // Widgets de volume
    lv_obj_t* volumeSlider_;
    lv_obj_t* volumeLabel_;

    // Widgets de brilho
    lv_obj_t* brightnessSlider_;
    lv_obj_t* brightnessLabel_;

    // Info do sistema
    lv_obj_t* fwVersionLabel_;
    lv_obj_t* uptimeLabel_;
    lv_obj_t* memoryLabel_;

    // Botao de voltar
    lv_obj_t* backBtn_;

    // Flag para prevenir feedback loop em atualizacoes externas
    bool updatingFromExternal_;

    // Controle de refresh de info (atualiza a cada 1s, nao a cada 5ms)
    uint32_t lastInfoUpdate_;

    // Callbacks estaticos para LVGL (recebem this via user_data)
    static void onVolumeChanged(lv_event_t* e);
    static void onBrightnessChanged(lv_event_t* e);
    static void onBackClicked(lv_event_t* e);
};

#endif // UI_SCREENS_SETTINGS_SCREEN_H
