/**
 * ============================================================================
 * WIDGET BARRA DE STATUS - HEADER
 * ============================================================================
 *
 * Componente reutilizavel para barra de status.
 * Vive em lv_layer_top() para persistir entre transicoes de tela.
 * Inclui botoes de menu (navegar telas) e voltar (pop na pilha).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef UI_STATUS_BAR_H
#define UI_STATUS_BAR_H

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus

// Forward declarations
class IScreenManager;

/**
 * Dados para atualizar a barra de status
 */
struct StatusBarData {
    bool ignicaoOn;
    uint32_t tempoIgnicao;
    uint32_t tempoJornada;
    const char* mensagem;
};

/**
 * Widget de barra de status
 *
 * Criado em lv_layer_top() para persistir entre transicoes de tela.
 * Contem: botao voltar, botao menu, indicador de ignicao, timers, mensagem.
 */
class StatusBar {
public:
    StatusBar();
    ~StatusBar();

    /**
     * Cria a barra de status em lv_layer_top()
     * Nao precisa de parent -- usa lv_layer_top() automaticamente.
     */
    void create();

    /**
     * Destroi a barra de status
     */
    void destroy();

    /**
     * Atualiza a barra de status
     * @param data Dados para exibir
     */
    void update(const StatusBarData& data);

    /**
     * Atualiza apenas o indicador de ignicao
     * @param on true se ignicao ligada
     * @param tempo Tempo em ms
     */
    void setIgnicao(bool on, uint32_t tempo);

    /**
     * Define mensagem na barra
     * @param message Texto da mensagem
     * @param color Cor do texto
     * @param font Fonte (opcional)
     * @param timeoutMs Tempo para expirar (0 = permanente)
     */
    void setMessage(const char* message,
                    lv_color_t color = lv_color_hex(0x888888),
                    const lv_font_t* font = &lv_font_montserrat_20,
                    uint32_t timeoutMs = 0);

    /**
     * Limpa a mensagem
     */
    void clearMessage();

    /**
     * Verifica se a barra foi criada
     */
    bool isCreated() const { return container_ != nullptr; }

    /**
     * Obtem o container principal
     */
    lv_obj_t* getContainer() const { return container_; }

    /**
     * Define o ponteiro para o gerenciador de telas
     * Usado internamente para o callback do botao trocar tela.
     * @param mgr Ponteiro para IScreenManager
     */
    void setScreenManager(IScreenManager* mgr);

private:
    // Callbacks LVGL
    static void updateTimerCallback(lv_timer_t* timer);
    static void swapBtnCallback(lv_event_t* e);

    // Elementos UI
    lv_obj_t* container_;
    lv_obj_t* swapBtn_;
    lv_obj_t* ignicaoIndicator_;
    lv_obj_t* ignicaoLabel_;
    lv_obj_t* tempoIgnicaoLabel_;
    lv_obj_t* tempoJornadaLabel_;
    lv_obj_t* mensagemLabel_;

    // Timer
    lv_timer_t* updateTimer_;

    // Estado
    uint32_t messageExpireTime_;

    // Referencia para o gerenciador de telas (loose coupling)
    IScreenManager* screenManager_;
};

#endif // __cplusplus

#endif // UI_STATUS_BAR_H
