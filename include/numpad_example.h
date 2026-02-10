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

// Forward declaration
class StatusBar;

// ============================================================================
// CLASSE NUMPAD EXAMPLE
// ============================================================================

class NumpadExample {
private:
    ButtonManager* btnManager;
    StatusBar* statusBar_;
    std::string currentNumber;
    int maxDigits;
    
    // IDs dos botões
    int btnIds[12];  // 0-9, OK, CANCEL
    
    // Label para mostrar o número digitado
    lv_obj_t* displayLabel;
    
    // Timer para timeout
    lv_timer_t* timeoutTimer;
    unsigned long lastDigitTime;  // Timestamp do último dígito digitado
    
    // Callback do timer de timeout
    static void timeoutTimerCallback(lv_timer_t* timer);

    // Gerenciamento do timer (startTimeoutTimer privado, stop publico)
    void startTimeoutTimer();

public:
    NumpadExample();
    ~NumpadExample();

    // Inicialização
    void init(ButtonManager* mgr);
    void createNumpad();
    void clearNumpad();
    
    // Parar timer de timeout (chamado pelo ScreenManager ao sair da tela)
    void stopTimeoutTimer();

    // StatusBar persistente para preview dos digitos
    void setStatusBar(StatusBar* bar);

    // Lifecycle da tela (chamado pelo NumpadScreen)
    void onExitScreen();

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

#endif // NUMPAD_EXAMPLE_H