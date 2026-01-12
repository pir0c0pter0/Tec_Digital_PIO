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
    
    // Botões da jornada - cada um com seus próprios motoristas
    std::array<BotaoJornada, ACAO_MAX> botoes;
    
    // Popup de seleção de motorista
    lv_obj_t* popupMotorista;
    TipoAcao acaoPendente;
    
    // Singleton
    static JornadaKeyboard* instance;
    
    // Callbacks
    static void onActionButtonClick(int buttonId);
    static void onMotoristaSelectClick(lv_event_t* e);
    static void onCancelPopupClick(lv_event_t* e);
    
    // Métodos internos
    void showMotoristaSelection(TipoAcao acao);
    void closeMotoristaSelection();
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
    
    // Singleton
    static JornadaKeyboard* getInstance();
    
    // Inicialização
    void init();
    void createKeyboard();
    void clearKeyboard();
    
    // Estado dos motoristas POR BOTÃO
    bool isMotoristaLogado(TipoAcao acao, int motorista);
    void logarMotorista(TipoAcao acao, int motorista);
    void deslogarMotorista(TipoAcao acao, int motorista);
    
    // Obter estado atual
    EstadoMotorista getEstadoMotorista(TipoAcao acao, int motorista);
};

// ============================================================================
// CLASSE SCREEN MANAGER - GERENCIADOR DE TELAS
// ============================================================================

class ScreenManager {
private:
    enum TelaAtiva {
        TELA_NUMPAD = 0,
        TELA_JORNADA = 1
    };
    
    TelaAtiva telaAtual;
    lv_obj_t* container;
    
    // Instâncias das telas
    void* numpadInstance;  // NumpadExample*
    JornadaKeyboard* jornadaInstance;
    
    // Botão de troca de tela
    lv_obj_t* btnScreenSwitch;
    
    // Singleton
    static ScreenManager* instance;
    
    // Callbacks
    static void onScreenSwitchClick(lv_event_t* e);
    
    // Criar botão de troca de tela
    void createScreenSwitchButton();
    
public:
    ScreenManager();
    ~ScreenManager();
    
    static ScreenManager* getInstance();
    
    void init();
    void showNumpadScreen();
    void showJornadaScreen();
    void toggleScreen();
    
    TelaAtiva getTelaAtual() const { return telaAtual; }
};

// ============================================================================
// FUNÇÕES HELPER (extern C para acesso de main.cpp)
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa o gerenciador de telas
 */
void initScreenManager();

/**
 * Mostra tela do teclado numérico
 */
void showNumpadKeyboard();

/**
 * Mostra tela do teclado de jornada
 */
void showJornadaKeyboard();

/**
 * Alterna entre os teclados (para teste)
 */
void toggleKeyboard();

#ifdef __cplusplus
}
#endif

#endif // JORNADA_KEYBOARD_H