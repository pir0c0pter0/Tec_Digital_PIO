/**
 * ============================================================================
 * TELA DE OTA - HEADER
 * ============================================================================
 *
 * Implementacao de IScreen para tela de atualizacao de firmware (OTA).
 * Exibe barra de progresso, percentual, bytes transferidos e estado
 * atual do processo OTA em portugues.
 *
 * Nao usa ButtonManager -- cria widgets LVGL diretamente na tela.
 * Sem elementos interativos (UI travada durante OTA conforme OTA-08).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef UI_SCREENS_OTA_SCREEN_H
#define UI_SCREENS_OTA_SCREEN_H

#include "interfaces/i_screen.h"
#include "services/ota/ota_types.h"
#include "config/app_config.h"
#include "lvgl.h"

/**
 * Tela de atualizacao OTA: progresso + estado.
 *
 * Layout (480x320, 40px status bar):
 * - Titulo "Atualizacao de Firmware" centralizado
 * - Label de estado OTA (texto em portugues)
 * - Barra de progresso (440px largura)
 * - Label de percentual ("XX%")
 * - Label de bytes ("XXX / YYY KB")
 * - Label de erro (oculto por padrao, vermelho)
 *
 * Sem botao de voltar (UI locked durante OTA per OTA-08).
 */
class OtaScreen : public IScreen {
public:
    OtaScreen();
    ~OtaScreen() override;

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
     * Atualiza barra de progresso e labels de percentual/bytes.
     * @param percent Progresso 0-100%
     * @param received Bytes recebidos
     * @param total Bytes totais
     */
    void updateProgress(uint8_t percent, uint32_t received, uint32_t total);

    /**
     * Atualiza label de estado OTA com texto em portugues.
     * @param state Estado atual do OTA
     */
    void updateState(OtaState state);

    /**
     * Exibe mensagem de erro em vermelho.
     * @param message Mensagem de erro
     */
    void showError(const char* message);

private:
    // Tela LVGL
    lv_obj_t* screen_;
    bool created_;

    // Widgets
    lv_obj_t* titleLabel_;
    lv_obj_t* stateLabel_;
    lv_obj_t* progressBar_;
    lv_obj_t* percentLabel_;
    lv_obj_t* bytesLabel_;
    lv_obj_t* errorLabel_;

    /**
     * Converte OtaState para string em portugues.
     * @param state Estado OTA
     * @return String descritiva em portugues
     */
    static const char* stateToString(OtaState state);
};

#endif // UI_SCREENS_OTA_SCREEN_H
