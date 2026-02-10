/**
 * ============================================================================
 * EXEMPLO DE USO DO SISTEMA ROBUSTO DE CRIAÇÃO DE BOTÕES
 * ============================================================================
 */

#include "numpad_example.h"
#include "button_manager.h"  // Agora com o sistema robusto
#include "ui/widgets/status_bar.h"
#include "esp_bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdio>

static const char *TAG = "NUMPAD";

// Macros de compatibilidade Arduino -> ESP-IDF
#define millis() ((uint32_t)(esp_timer_get_time() / 1000ULL))

// Declaração externa para tocar áudio
extern "C" void playAudioFile(const char* filename);

// ============================================================================
// CONFIGURAÇÕES
// ============================================================================
#define NUMPAD_TIMEOUT_MS 10000  // 10 segundos

// ============================================================================
// VARIÁVEIS ESTÁTICAS
// ============================================================================
NumpadExample* NumpadExample::instance = nullptr;
static NumpadExample* g_numpadInstance = nullptr;

// ============================================================================
// CONSTRUTOR E DESTRUTOR
// ============================================================================

NumpadExample::NumpadExample() :
    btnManager(nullptr),
    statusBar_(nullptr),
    currentNumber(""),
    maxDigits(11),
    displayLabel(nullptr),
    timeoutTimer(nullptr),
    lastDigitTime(0) {
    
    for (int i = 0; i < 12; i++) {
        btnIds[i] = -1;
    }
}

NumpadExample::~NumpadExample() {
    clearNumpad();
}

// ============================================================================
// SINGLETON
// ============================================================================

NumpadExample* NumpadExample::getInstance() {
    if (instance == nullptr) {
        instance = new NumpadExample();
    }
    return instance;
}

// ============================================================================
// INICIALIZAÇÃO
// ============================================================================

void NumpadExample::init() {
    btnManager = ButtonManager::getInstance();
    if (!btnManager) {
        esp_rom_printf("ERRO: ButtonManager não inicializado!");
        return;
    }

    esp_rom_printf("NumpadExample inicializado com sistema robusto");
}

void NumpadExample::init(ButtonManager* mgr) {
    btnManager = mgr;
    if (!btnManager) {
        esp_rom_printf("ERRO: ButtonManager nulo passado para NumpadExample!");
        return;
    }

    esp_rom_printf("NumpadExample inicializado com ButtonManager externo");
}

// ============================================================================
// CRIAÇÃO DO NUMPAD COM SISTEMA ROBUSTO
// ============================================================================

void NumpadExample::createNumpad() {
    if (!btnManager) {
        esp_rom_printf("ERRO: ButtonManager não disponível");
        return;
    }
    
    // Limpa teclado anterior se existir
    clearNumpad();
    currentNumber = "";
    lastDigitTime = 0;
    g_numpadInstance = this;
    
    esp_rom_printf("==============================================");
    esp_rom_printf("  CRIANDO TECLADO NUMÉRICO (SISTEMA ROBUSTO)");
    esp_rom_printf("==============================================");
    
    // ========================================================================
    // MÉTODO 1: CRIAÇÃO EM LOTE (RECOMENDADO)
    // ========================================================================
    
    std::vector<ButtonManager::ButtonBatchDef> buttonDefs = {
        // Primeira linha: 1, 2, 3, CANCELAR
        {0, 0, "1", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick, 
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        {1, 0, "2", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick,
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        {2, 0, "3", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick,
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        {3, 0, "CANCELAR", ICON_CANCEL, nullptr, lv_color_hex(0xFF4444), onCancelClick,
         1, 1, lv_color_hex(0x000000), &lv_font_montserrat_16},
        
        // Segunda linha: 4, 5, 6, 0
        {0, 1, "4", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick,
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        {1, 1, "5", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick,
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        {2, 1, "6", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick,
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        {3, 1, "0", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick,
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        // Terceira linha: 7, 8, 9, ENVIAR
        {0, 2, "7", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick,
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        {1, 2, "8", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick,
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        {2, 2, "9", ICON_NONE, nullptr, lv_color_hex(0x4444FF), onDigitClick,
         1, 1, lv_color_hex(0xFFFFFF), &lv_font_montserrat_42},
        
        {3, 2, "ENVIAR", ICON_CHECK, nullptr, lv_color_hex(0x44FF44), onOkClick,
         1, 1, lv_color_hex(0x000000), &lv_font_montserrat_16}
    };
    
    // Criar todos os botões em lote (com retry automático)
    esp_rom_printf("📦 Iniciando criação em lote de 12 botões...");
    std::vector<int> ids = btnManager->addButtonBatch(buttonDefs);
    
    // Mapear IDs para os arrays internos
    btnIds[1] = ids[0];   // Botão "1"
    btnIds[2] = ids[1];   // Botão "2"
    btnIds[3] = ids[2];   // Botão "3"
    btnIds[10] = ids[3];  // CANCELAR
    btnIds[4] = ids[4];   // Botão "4"
    btnIds[5] = ids[5];   // Botão "5"
    btnIds[6] = ids[6];   // Botão "6"
    btnIds[0] = ids[7];   // Botão "0"
    btnIds[7] = ids[8];   // Botão "7"
    btnIds[8] = ids[9];   // Botão "8"
    btnIds[9] = ids[10];  // Botão "9"
    btnIds[11] = ids[11]; // ENVIAR
    
    // ========================================================================
    // VERIFICAÇÃO DE CRIAÇÃO
    // ========================================================================
    
    esp_rom_printf("\n🔍 Verificando criação dos botões...");
    
    // Aguardar confirmação de criação (com timeout de 1 segundo)
    if (btnManager->waitForAllButtons(ids, 1000)) {
        esp_rom_printf("✅ SUCESSO: Todos os botões foram criados!");
        
        // Verificar status individual para debug
        esp_rom_printf("\n📊 Status individual dos botões:");
        const char* labels[] = {"1", "2", "3", "CANCELAR", "4", "5", "6", "0", "7", "8", "9", "ENVIAR"};
        for (size_t i = 0; i < ids.size(); i++) {
            auto status = btnManager->getButtonCreationStatus(ids[i]);
            const char* statusStr = "";
            const char* emoji = "";
            
            switch(status) {
                case ButtonManager::CREATION_SUCCESS:
                    statusStr = "CRIADO";
                    emoji = "✓";
                    break;
                case ButtonManager::CREATION_PENDING:
                    statusStr = "PENDENTE";
                    emoji = "⏳";
                    break;
                case ButtonManager::CREATION_FAILED:
                    statusStr = "FALHOU";
                    emoji = "✗";
                    break;
            }
            
            esp_rom_printf("  %s Botão '%s' (ID: %d): %s\n", 
                         emoji, labels[i], ids[i], statusStr);
        }
        
    } else {
        esp_rom_printf("⚠️ AVISO: Alguns botões podem não ter sido criados");
        esp_rom_printf("📊 Botões pendentes: %d\n", btnManager->getPendingButtonCount());
        
        // Diagnóstico detalhado
        esp_rom_printf("\n🔧 Diagnóstico detalhado:");
        for (size_t i = 0; i < ids.size(); i++) {
            auto status = btnManager->getButtonCreationStatus(ids[i]);
            if (status != ButtonManager::CREATION_SUCCESS) {
                esp_rom_printf("  ⚠ Botão ID %d está %s\n", 
                             ids[i], 
                             (status == ButtonManager::CREATION_PENDING) ? "PENDENTE" : "COM FALHA");
            }
        }
        
        // Aguardar mais um pouco se houver pendentes
        if (btnManager->getPendingButtonCount() > 0) {
            esp_rom_printf("\n⏳ Aguardando processamento de pendentes...");
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // Verificar novamente
            int pendingCount = btnManager->getPendingButtonCount();
            if (pendingCount == 0) {
                esp_rom_printf("✅ Todos os botões pendentes foram processados!");
            } else {
                esp_rom_printf("⚠ Ainda há %d botões pendentes\n", pendingCount);
            }
        }
    }
    
    // ========================================================================
    // MÉTODO 2: CRIAÇÃO INDIVIDUAL (ALTERNATIVA)
    // ========================================================================
    /*
    // Se preferir criar individualmente (também com retry automático):
    btnIds[1] = btnManager->addButton(0, 0, "1", ICON_NONE, nullptr,
                                      lv_color_hex(0x4444FF), onDigitClick, 1, 1,
                                      lv_color_hex(0xFFFFFF), &lv_font_montserrat_42);
    
    // ... resto dos botões ...
    
    // Verificar criação individual
    for (int i = 0; i <= 11; i++) {
        if (btnIds[i] != -1) {
            auto status = btnManager->getButtonCreationStatus(btnIds[i]);
            if (status == ButtonManager::CREATION_SUCCESS) {
                esp_rom_printf("✓ Botão %d criado com sucesso\n", i);
            } else {
                esp_rom_printf("⚠ Botão %d está %s\n", i,
                             (status == ButtonManager::CREATION_PENDING) ? "pendente" : "com falha");
            }
        }
    }
    */
    
    // ========================================================================
    // FINALIZAÇÃO
    // ========================================================================
    
    // Iniciar timer de timeout
    startTimeoutTimer();
    
    // Atualizar display
    updateDisplay();
    
    esp_rom_printf("\n==============================================");
    esp_rom_printf("  TECLADO NUMÉRICO PRONTO PARA USO");
    esp_rom_printf("==============================================\n");
}

// ============================================================================
// LIMPEZA DO NUMPAD
// ============================================================================

void NumpadExample::clearNumpad() {
    if (!btnManager) return;
    
    stopTimeoutTimer();
    g_numpadInstance = nullptr;
    
    esp_rom_printf("🧹 Removendo teclado numérico...");
    
    // O ButtonManager agora garante remoção segura
    for (int i = 0; i < 12; i++) {
        if (btnIds[i] != -1) {
            btnManager->removeButton(btnIds[i]);
            btnIds[i] = -1;
        }
    }
    
    displayLabel = nullptr;
    currentNumber = "";
    lastDigitTime = 0;
    
    esp_rom_printf("✓ Teclado numérico removido com segurança");
}

// ============================================================================
// RESTO DA IMPLEMENTAÇÃO (callbacks, etc.)
// ============================================================================

void NumpadExample::resetToInitialMessage() {
    if (statusBar_) {
        statusBar_->setMessage("Digite o codigo",
                               lv_color_hex(0x888888),
                               &lv_font_montserrat_16);
    }
}

void NumpadExample::startTimeoutTimer() {
    if (timeoutTimer) {
        lv_timer_del(timeoutTimer);
    }
    timeoutTimer = lv_timer_create(timeoutTimerCallback, 1000, NULL);
    esp_rom_printf("⏰ Timer de timeout iniciado");
}

void NumpadExample::stopTimeoutTimer() {
    if (timeoutTimer) {
        lv_timer_del(timeoutTimer);
        timeoutTimer = nullptr;
        esp_rom_printf("⏰ Timer de timeout parado");
    }
}

void NumpadExample::setStatusBar(StatusBar* bar) {
    statusBar_ = bar;
}

void NumpadExample::onExitScreen() {
    currentNumber = "";
    lastDigitTime = 0;
    stopTimeoutTimer();
    if (statusBar_) {
        statusBar_->clearMessage();
    }
}

void NumpadExample::timeoutTimerCallback(lv_timer_t* timer) {
    NumpadExample* numpad = g_numpadInstance;
    if (!numpad) return;
    
    if (numpad->currentNumber.empty()) {
        return;
    }
    
    unsigned long currentTime = millis();
    unsigned long timeSinceLastDigit = currentTime - numpad->lastDigitTime;
    
    if (timeSinceLastDigit >= NUMPAD_TIMEOUT_MS) {
        esp_rom_printf("⏱️ TIMEOUT: Limpando número");

        playAudioFile("/nok_click.mp3");
        numpad->currentNumber = "";
        numpad->lastDigitTime = 0;

        if (numpad->statusBar_) {
            numpad->statusBar_->setMessage("Timeout - Codigo limpo",
                                           lv_color_hex(0xFF4444),
                                           &lv_font_montserrat_18,
                                           3000);
        }
    }
}

// ============================================================================
// OPERAÇÕES
// ============================================================================

void NumpadExample::addDigit(int digit) {
    if (currentNumber.length() >= maxDigits) {
        esp_rom_printf("Numero maximo de digitos atingido");

        if (statusBar_) {
            statusBar_->setMessage("Maximo 11 digitos!",
                                   lv_color_hex(0xFFAA00),
                                   &lv_font_montserrat_16,
                                   2000);
        }
        playAudioFile("/nok_click.mp3");
        return;
    }
    
    currentNumber += std::to_string(digit);
    
    // Atualiza tempo do último dígito
    lastDigitTime = millis();
    
    updateDisplay();
    
    // Toca som de clique
    playAudioFile("/click.mp3");
    
    esp_rom_printf("Digito adicionado: %d, Numero atual: %s, Tempo: %lu\n", 
                  digit, currentNumber.c_str(), lastDigitTime);
}

void NumpadExample::removeLastDigit() {
    if (!currentNumber.empty()) {
        currentNumber.pop_back();
        
        // Atualiza tempo
        if (!currentNumber.empty()) {
            lastDigitTime = millis();
        } else {
            lastDigitTime = 0;
        }
        
        updateDisplay();
        esp_rom_printf("Ultimo digito removido, Numero atual: %s\n", 
                      currentNumber.c_str());
    }
}

void NumpadExample::clearNumber() {
    currentNumber = "";
    lastDigitTime = 0;
    updateDisplay();
    esp_rom_printf("Numero limpo");
}

void NumpadExample::updateDisplay() {
    if (!statusBar_) return;

    if (currentNumber.empty()) {
        // Mensagem inicial em cinza claro
        statusBar_->setMessage("Digite o codigo",
                               lv_color_hex(0x888888),
                               &lv_font_montserrat_16);
    } else {
        // Preview do numero digitado em branco, tamanho grande
        statusBar_->setMessage(currentNumber.c_str(),
                               lv_color_hex(0xFFFFFF),
                               &lv_font_montserrat_24);
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void NumpadExample::onDigitClick(int buttonId) {
    NumpadExample* numpad = getInstance();
    if (!numpad) return;
    
    // Identifica qual dígito foi pressionado baseado no ID do botão
    for (int i = 0; i <= 9; i++) {
        if (numpad->btnIds[i] == buttonId) {
            numpad->addDigit(i);
            break;
        }
    }
}

void NumpadExample::onOkClick(int buttonId) {
    NumpadExample* numpad = getInstance();
    if (!numpad || !numpad->btnManager) return;
    
    std::string number = numpad->getNumber();
    
    // Verifica se há número digitado
    if (number.empty()) {
        playAudioFile("/nok_click.mp3");
        if (numpad->statusBar_) {
            numpad->statusBar_->setMessage("Nenhum numero digitado!",
                                           lv_color_hex(0xFFFF00),
                                           &lv_font_montserrat_16,
                                           2500);
        }
        numpad->btnManager->showPopup("Aviso",
                                      "Nenhum numero digitado!",
                                      POPUP_WARNING, false, nullptr);
        return;
    }
    
    // Verifica se é apenas zeros
    bool isOnlyZeros = true;
    for (char c : number) {
        if (c != '0') {
            isOnlyZeros = false;
            break;
        }
    }
    
    if (isOnlyZeros) {
        playAudioFile("/nok_click.mp3");
        if (numpad->statusBar_) {
            numpad->statusBar_->setMessage("Numero nao pode ser zero!",
                                           lv_color_hex(0xFF0000),
                                           &lv_font_montserrat_16,
                                           3000);
        }
        numpad->btnManager->showPopup("Erro",
                                      "Numero nao pode ser zero!",
                                      POPUP_ERROR, false, nullptr);
        esp_rom_printf("ERRO: Numero zero nao permitido");
        return;
    }
    
    // Número válido - continua...
    playAudioFile("/ok_click.mp3");

    if (numpad->statusBar_) {
        char successMsg[100];
        snprintf(successMsg, sizeof(successMsg), "Codigo %s enviado!", number.c_str());
        numpad->statusBar_->setMessage(successMsg,
                                       lv_color_hex(0x00FF00),
                                       &lv_font_montserrat_18,
                                       3000);
    }
    
    char message[100];
    snprintf(message, sizeof(message), 
             "Codigo enviado:\n\n%s", number.c_str());
    
    numpad->btnManager->showPopup("Sucesso", 
                                  message,
                                  POPUP_SUCCESS, false, nullptr);
    
    numpad->clearNumber();
    
    esp_rom_printf("Numero enviado com sucesso: %s\n", number.c_str());
}

void NumpadExample::onCancelClick(int buttonId) {
    NumpadExample* numpad = getInstance();
    if (!numpad || !numpad->btnManager) return;
    
    // Toca som de cancelamento
    playAudioFile("/nok_click.mp3");

    // Limpa os números
    numpad->clearNumber();

    // Mensagem de cancelamento em cinza escuro com timeout
    if (numpad->statusBar_) {
        numpad->statusBar_->setMessage("Operacao cancelada",
                                       lv_color_hex(0x666666),
                                       &lv_font_montserrat_16,
                                       2000);
    }
    
    esp_rom_printf("Cancelar pressionado - numeros limpos");
}

// ============================================================================
// FUNÇÕES HELPER GLOBAIS
// ============================================================================

void showNumpad() {
    esp_rom_printf("Mostrando teclado numerico");
    
    ButtonManager* mgr = ButtonManager::getInstance();
    if (mgr) {
        // Limpa todos os botões existentes
        mgr->removeAllButtons();
    }
    
    NumpadExample* numpad = NumpadExample::getInstance();
    numpad->init();
    numpad->createNumpad();
}

void hideNumpad() {
    esp_rom_printf("Escondendo teclado numerico");
    
    NumpadExample* numpad = NumpadExample::getInstance();
    numpad->clearNumpad();
    
    ButtonManager* mgr = ButtonManager::getInstance();
    if (mgr) {
        mgr->setStatusMessage("Teclado fechado",
                            lv_color_hex(0x888888),  // Cinza
                            &lv_font_montserrat_16,   // Tamanho médio
                            2000);                    // 2 segundos
    }
}

// ============================================================================
// WRAPPERS EXTERN C PARA CHAMADAS DE main.cpp
// ============================================================================

extern "C" {

void numpadInit(void) {
    NumpadExample* numpad = NumpadExample::getInstance();
    numpad->init();
}

} // extern "C"

// ============================================================================
// FIM DA IMPLEMENTAÇÃO
// ============================================================================