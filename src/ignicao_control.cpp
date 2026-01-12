/**
 * ============================================================================
 * CONTROLE DE IGNIÇÃO - IMPLEMENTAÇÃO (ESP-IDF)
 * ============================================================================
 */

#include "ignicao_control.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "IGNICAO";

// Macros de compatibilidade Arduino -> ESP-IDF
#define millis() ((uint32_t)(esp_timer_get_time() / 1000ULL))
#define INPUT_PULLDOWN GPIO_MODE_INPUT
#define HIGH 1
#define LOW 0

// Função para configurar pino GPIO
static void pinMode(int pin, int mode) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

// Função para ler pino GPIO
static int digitalRead(int pin) {
    return gpio_get_level((gpio_num_t)pin);
}

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

volatile bool ignicaoStatus = false;
float debounceIgnicaoOn = 2.0;
float debounceIgnicaoOff = 3.0;
SemaphoreHandle_t ignicaoMutex = NULL;
TaskHandle_t ignicaoTaskHandle = NULL;

unsigned long totalIgnicaoOnTime = 0;
unsigned long totalIgnicaoOffTime = 0;
unsigned long lastIgnicaoChangeTime = 0;

// ============================================================================
// VARIÁVEIS INTERNAS
// ============================================================================

static unsigned long debounceStartTime = 0;
static bool lastPinState = false;
static bool debounceInProgress = false;
static bool targetState = false;
static unsigned long statisticsStartTime = 0;

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

/**
 * Inicializa o sistema de controle de ignição
 */
bool initIgnicaoControl(float debounceOn, float debounceOff, bool startTask) {
    if (debounceOn < 0 || debounceOff < 0) {
        return false;
    }
    
    debounceIgnicaoOn = debounceOn;
    debounceIgnicaoOff = debounceOff;
    
    pinMode(IGNICAO_PIN, INPUT_PULLDOWN);
    
    if (ignicaoMutex == NULL) {
        ignicaoMutex = xSemaphoreCreateMutex();
        if (ignicaoMutex == NULL) {
            return false;
        }
    }
    
    bool initialState = digitalRead(IGNICAO_PIN);
    ignicaoStatus = initialState;
    lastPinState = initialState;
    
    statisticsStartTime = millis();
    lastIgnicaoChangeTime = millis();
    
    ESP_LOGI(TAG, "Ignicao inicial: %s", initialState ? "ON" : "OFF");
    
    if (startTask) {
        if (ignicaoTaskHandle == NULL) {
            BaseType_t xReturned = xTaskCreatePinnedToCore(
                ignicaoMonitorTask,
                "IgnicaoMonitor",
                4096,
                NULL,
                2,
                &ignicaoTaskHandle,
                0
            );
            
            if (xReturned != pdPASS) {
                return false;
            }
        }
    }
    
    return true;
}

/**
 * Obtém o status atual da ignição
 */
bool getIgnicaoStatus() {
    bool status = false;
    
    if (xSemaphoreTake(ignicaoMutex, portMAX_DELAY) == pdTRUE) {
        status = ignicaoStatus;
        xSemaphoreGive(ignicaoMutex);
    }
    
    return status;
}

/**
 * Define novos tempos de debounce
 */
void setDebounceTime(float debounceOn, float debounceOff) {
    if (debounceOn < 0 || debounceOff < 0) {
        return;
    }
    
    if (xSemaphoreTake(ignicaoMutex, portMAX_DELAY) == pdTRUE) {
        debounceIgnicaoOn = debounceOn;
        debounceIgnicaoOff = debounceOff;
        debounceInProgress = false;
        xSemaphoreGive(ignicaoMutex);
    }
}

/**
 * Obtém os tempos de debounce atuais
 */
void getDebounceTime(float* debounceOn, float* debounceOff) {
    if (debounceOn == NULL || debounceOff == NULL) return;
    
    if (xSemaphoreTake(ignicaoMutex, portMAX_DELAY) == pdTRUE) {
        *debounceOn = debounceIgnicaoOn;
        *debounceOff = debounceIgnicaoOff;
        xSemaphoreGive(ignicaoMutex);
    }
}

/**
 * Task de monitoramento da ignição
 */
void ignicaoMonitorTask(void* parameter) {
    while (true) {
        bool currentPinState = digitalRead(IGNICAO_PIN);
        
        if (xSemaphoreTake(ignicaoMutex, portMAX_DELAY) == pdTRUE) {
            unsigned long currentTime = millis();
            
            // Verifica mudança no pino
            if (currentPinState != lastPinState) {
                lastPinState = currentPinState;
                debounceStartTime = currentTime;
                debounceInProgress = true;
                targetState = currentPinState;
            }
            
            // Processa debounce
            if (debounceInProgress) {
                float requiredDebounce = targetState ? debounceIgnicaoOn : debounceIgnicaoOff;
                unsigned long debounceMillis = (unsigned long)(requiredDebounce * 1000);
                
                if (requiredDebounce == 0 || (currentTime - debounceStartTime) >= debounceMillis) {
                    bool confirmState = digitalRead(IGNICAO_PIN);
                    
                    if (confirmState == targetState && ignicaoStatus != targetState) {
                        // Atualiza estatísticas
                        unsigned long duration = currentTime - lastIgnicaoChangeTime;
                        if (ignicaoStatus) {
                            totalIgnicaoOnTime += duration;
                        } else {
                            totalIgnicaoOffTime += duration;
                        }
                        
                        // Atualiza status
                        ignicaoStatus = targetState;
                        lastIgnicaoChangeTime = currentTime;
                        debounceInProgress = false;
                        
                        ESP_LOGI(TAG, "Ignicao mudou para: %s", targetState ? "ON" : "OFF");
                        
                        // Libera mutex ANTES do callback
                        xSemaphoreGive(ignicaoMutex);
                        
                        // Chama callback
                        onIgnicaoStatusChange(targetState);
                        
                        // Delay antes de continuar
                        vTaskDelay(pdMS_TO_TICKS(50));
                        continue;
                    } else if (confirmState != targetState) {
                        debounceInProgress = false;
                        lastPinState = confirmState;
                    } else {
                        debounceInProgress = false;
                    }
                }
            }
            
            xSemaphoreGive(ignicaoMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(IGNICAO_CHECK_INTERVAL_MS));
    }
}

/**
 * Para a task de monitoramento
 */
void stopIgnicaoMonitor() {
    if (ignicaoTaskHandle != NULL) {
        vTaskDelete(ignicaoTaskHandle);
        ignicaoTaskHandle = NULL;
    }
}

/**
 * Reinicia a task de monitoramento
 */
void restartIgnicaoMonitor() {
    stopIgnicaoMonitor();
    
    if (ignicaoMutex != NULL) {
        BaseType_t xReturned = xTaskCreatePinnedToCore(
            ignicaoMonitorTask,
            "IgnicaoMonitor",
            4096,
            NULL,
            2,
            &ignicaoTaskHandle,
            0
        );
        
        if (xReturned != pdPASS) {
            ESP_LOGI(TAG, "Erro ao reiniciar task de ignicao");
        }
    }
}

/**
 * Obtém estatísticas de uso
 */
void getIgnicaoStatistics(unsigned long* onTime, unsigned long* offTime, 
                          unsigned long* lastChange) {
    if (xSemaphoreTake(ignicaoMutex, portMAX_DELAY) == pdTRUE) {
        unsigned long currentTime = millis();
        unsigned long currentSessionDuration = currentTime - lastIgnicaoChangeTime;
        
        if (onTime != NULL) {
            *onTime = totalIgnicaoOnTime;
            if (ignicaoStatus) {
                *onTime += currentSessionDuration;
            }
        }
        
        if (offTime != NULL) {
            *offTime = totalIgnicaoOffTime;
            if (!ignicaoStatus) {
                *offTime += currentSessionDuration;
            }
        }
        
        if (lastChange != NULL) {
            *lastChange = lastIgnicaoChangeTime;
        }
        
        xSemaphoreGive(ignicaoMutex);
    }
}

/**
 * Reseta as estatísticas
 */
void resetIgnicaoStatistics() {
    if (xSemaphoreTake(ignicaoMutex, portMAX_DELAY) == pdTRUE) {
        totalIgnicaoOnTime = 0;
        totalIgnicaoOffTime = 0;
        lastIgnicaoChangeTime = millis();
        statisticsStartTime = millis();
        xSemaphoreGive(ignicaoMutex);
    }
}

/**
 * Obtém o estado bruto do pino
 */
int getIgnicaoRawState() {
    return digitalRead(IGNICAO_PIN);
}

// ============================================================================
// FIM DA IMPLEMENTAÇÃO
// ============================================================================