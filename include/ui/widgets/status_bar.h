/**
 * ============================================================================
 * WIDGET BARRA DE STATUS - HEADER
 * ============================================================================
 *
 * Componente reutilizavel para barra de status.
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
 */
class StatusBar {
public:
    StatusBar();
    ~StatusBar();

    /**
     * Cria a barra de status
     * @param parent Objeto pai (normalmente a tela)
     */
    void create(lv_obj_t* parent);

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
    bool isCreated() const { return container != nullptr; }

    /**
     * Obtem o container principal
     */
    lv_obj_t* getContainer() const { return container; }

private:
    static void updateTimerCallback(lv_timer_t* timer);

    // Elementos UI
    lv_obj_t* container;
    lv_obj_t* ignicaoIndicator;
    lv_obj_t* ignicaoLabel;
    lv_obj_t* tempoIgnicaoLabel;
    lv_obj_t* tempoJornadaLabel;
    lv_obj_t* mensagemLabel;

    // Timer
    lv_timer_t* updateTimer;

    // Estado
    uint32_t messageExpireTime;
    lv_obj_t* parent;
};

#endif // __cplusplus

#endif // UI_STATUS_BAR_H
