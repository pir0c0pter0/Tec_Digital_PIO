/**
 * ============================================================================
 * TECLADO DE JORNADA - COM SISTEMA DE MÚLTIPLOS MOTORISTAS
 * ============================================================================
 */

#ifndef JORNADA_KEYBOARD_H
#define JORNADA_KEYBOARD_H

#include "button_manager.h"
#include <string>
#include <array>

// Forward declaration
class StatusBar;

// ============================================================================
// ENUMS E ESTRUTURAS
// ============================================================================

enum TipoAcao {
    ACAO_JORNADA = 0,
    ACAO_REFEICAO,
    ACAO_ESPERA,
    ACAO_MANOBRA,
    ACAO_CARGA,
    ACAO_DESCARGA,
    ACAO_ABASTECER,
    ACAO_DESCANSAR,
    ACAO_TRANSITO_PARADO,
    ACAO_POLICIA,
    ACAO_PANE,
    ACAO_EMERGENCIA,
    ACAO_MAX
};

struct EstadoMotorista {
    bool logado;
    unsigned long tempoInicio;
};

struct BotaoJornada {
    int buttonId;
    TipoAcao tipo;
    const char* label;
    ButtonIcon icon;
    lv_color_t color;
    lv_obj_t* indicadores[3];  // Labels dos pontos/asteriscos dos motoristas
    EstadoMotorista motoristas[3];  // Estado dos 3 motoristas PARA ESTE BOTÃO
    bool animacaoAtiva; 
};

// ============================================================================
// CLASSE JORNADA KEYBOARD
// ============================================================================

class JornadaKeyboard {
private:
    ButtonManager* btnManager;
    StatusBar* statusBar_;

    // Botões da jornada - cada um com seus próprios motoristas
    std::array<BotaoJornada, ACAO_MAX> botoes;

    // Popup de seleção de motorista
    lv_obj_t* popupMotorista;
    TipoAcao acaoPendente;

    // Callbacks (static porque LVGL requer ponteiros de funcao C-compatible)
    // Resolvem instancia via lv_obj user_data
    static void onMotoristaSelectClick(lv_event_t* e);
    static void onCancelPopupClick(lv_event_t* e);
    
    // Métodos internos
    void showMotoristaSelection(TipoAcao acao);
    void processarAcao(int motorista, TipoAcao acao);
    void atualizarIndicadores(TipoAcao acao);
    void atualizarTodosIndicadores();
    
    // Criar indicadores de motorista no botão
    void criarIndicadores(TipoAcao acao);
    
    // Obter ícone e cor para cada ação
    ButtonIcon getIconForAction(TipoAcao acao);
    lv_color_t getColorForAction(TipoAcao acao);
    const char* getLabelForAction(TipoAcao acao);

    void iniciarAnimacaoPulsante(TipoAcao acao);
    void pararAnimacaoPulsante(TipoAcao acao);
    
public:
    JornadaKeyboard();
    ~JornadaKeyboard();

    // StatusBar persistente (para mensagens de status)
    void setStatusBar(StatusBar* bar);

    // Inicialização
    void init(ButtonManager* mgr);
    void createKeyboard();
    void clearKeyboard();

    // Fechar popup de selecao de motorista (chamado pelo ScreenManager ao sair da tela)
    void closeMotoristaSelection();
    
    // Estado dos motoristas POR BOTÃO
    bool isMotoristaLogado(TipoAcao acao, int motorista);
    void logarMotorista(TipoAcao acao, int motorista);
    void deslogarMotorista(TipoAcao acao, int motorista);
    
    // Obter estado atual
    EstadoMotorista getEstadoMotorista(TipoAcao acao, int motorista);
};

#endif // JORNADA_KEYBOARD_H