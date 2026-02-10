/**
 * ============================================================================
 * EXEMPLO DE TECLADO NUMÉRICO - COM TIMEOUT
 * ============================================================================
 * 
 * Demonstração de uso do ButtonManager para criar um teclado numérico
 * com entrada de números, validação e timeout automático
 * 
 * ============================================================================
 */

#ifndef NUMPAD_EXAMPLE_H
#define NUMPAD_EXAMPLE_H

#include "button_manager.h"
#include <string>

// ============================================================================
// CLASSE NUMPAD EXAMPLE
// ============================================================================

class NumpadExample {
private:
    ButtonManager* btnManager;
    std::string currentNumber;
    int maxDigits;
    
    // IDs dos botões
    int btnIds[12];  // 0-9, OK, CANCEL
    
    // Label para mostrar o número digitado
    lv_obj_t* displayLabel;
    
    // Timer para timeout
    lv_timer_t* timeoutTimer;
    unsigned long lastDigitTime;  // Timestamp do último dígito digitado
    
    // Callbacks internos
    static void onDigitClick(int buttonId);
    static void onOkClick(int buttonId);
    static void onCancelClick(int buttonId);
    
    // Callback do timer de timeout
    static void timeoutTimerCallback(lv_timer_t* timer);

    // Gerenciamento do timer (startTimeoutTimer privado, stop publico)
    void startTimeoutTimer();
    
    // Singleton
    static NumpadExample* instance;
    
public:
    NumpadExample();
    ~NumpadExample();
    
    // Singleton
    static NumpadExample* getInstance();
    
    // Inicialização
    void init();
    void init(ButtonManager* mgr);
    void createNumpad();
    void clearNumpad();
    
    // Parar timer de timeout (chamado pelo ScreenManager ao sair da tela)
    void stopTimeoutTimer();

    // Operações
    void addDigit(int digit);
    void removeLastDigit();
    void clearNumber();
    std::string getNumber() const { return currentNumber; }
    
    // Atualização do display
    void updateDisplay();
    
    // Configurações
    void setMaxDigits(int max) { maxDigits = max; }

    void resetToInitialMessage();
};

// ============================================================================
// FUNÇÕES HELPER
// ============================================================================

/**
 * Cria e mostra o teclado numérico
 */
void showNumpad();

/**
 * Remove o teclado numérico
 */
void hideNumpad();

#endif // NUMPAD_EXAMPLE_H