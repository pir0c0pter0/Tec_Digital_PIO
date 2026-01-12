/**
 * ============================================================================
 * CONTROLE DE IGNIÇÃO - ESP32-S3
 * ============================================================================
 * 
 * Módulo responsável por monitorar o sinal de ignição na IO18 com debounce
 * configurável para evitar leituras falsas devido a ruídos elétricos.
 * 
 * Características:
 * - Monitoramento contínuo da IO18
 * - Debounce independente para ON e OFF
 * - Suporte a leitura instantânea (debounce = 0)
 * - Thread-safe com mutex para acesso multi-core
 * 
 * ============================================================================
 */

#ifndef IGNICAO_CONTROL_H
#define IGNICAO_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DEFINIÇÕES E CONSTANTES
// ============================================================================

// Pino de monitoramento da ignição
#define IGNICAO_PIN 18

// Estados da ignição
#define IGNICAO_OFF false
#define IGNICAO_ON  true

// Intervalo de verificação em ms (100ms = 10 verificações por segundo)
#define IGNICAO_CHECK_INTERVAL_MS 100

// ============================================================================
// VARIÁVEIS EXTERNAS
// ============================================================================

// Status atual da ignição (protegido por mutex)
extern volatile bool ignicaoStatus;

// Tempos de debounce em segundos (podem ser alterados em runtime)
extern float debounceIgnicaoOn;   // Tempo para confirmar ignição ligada
extern float debounceIgnicaoOff;  // Tempo para confirmar ignição desligada

// Mutex para acesso thread-safe
extern SemaphoreHandle_t ignicaoMutex;

// Handle da task de monitoramento
extern TaskHandle_t ignicaoTaskHandle;

// Estatísticas de monitoramento (opcional)
extern unsigned long totalIgnicaoOnTime;
extern unsigned long totalIgnicaoOffTime;
extern unsigned long lastIgnicaoChangeTime;

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

/**
 * Inicializa o sistema de controle de ignição.
 * 
 * @param debounceOn  Tempo em segundos para confirmar ignição ON (default: 2.0)
 * @param debounceOff Tempo em segundos para confirmar ignição OFF (default: 3.0)
 * @param startTask   Se true, inicia a task de monitoramento automaticamente
 * 
 * @return true se inicializado com sucesso, false em caso de erro
 */
bool initIgnicaoControl(float debounceOn = 2.0, float debounceOff = 3.0, bool startTask = true);

/**
 * Obtém o status atual da ignição de forma thread-safe.
 * 
 * @return true se ignição ON, false se OFF
 */
bool getIgnicaoStatus();

/**
 * Define novos tempos de debounce.
 * 
 * @param debounceOn  Tempo em segundos para confirmar ignição ON (0 = instantâneo)
 * @param debounceOff Tempo em segundos para confirmar ignição OFF (0 = instantâneo)
 */
void setDebounceTime(float debounceOn, float debounceOff);

/**
 * Obtém os tempos de debounce atuais.
 * 
 * @param debounceOn  Ponteiro para armazenar o tempo de debounce ON
 * @param debounceOff Ponteiro para armazenar o tempo de debounce OFF
 */
void getDebounceTime(float* debounceOn, float* debounceOff);

/**
 * Task de monitoramento da ignição (executada em loop).
 * Não deve ser chamada diretamente - use initIgnicaoControl().
 * 
 * @param parameter Parâmetro da task (não utilizado)
 */
void ignicaoMonitorTask(void* parameter);

/**
 * Para a task de monitoramento da ignição.
 */
void stopIgnicaoMonitor();

/**
 * Reinicia a task de monitoramento da ignição.
 */
void restartIgnicaoMonitor();

/**
 * Obtém estatísticas de uso da ignição.
 * 
 * @param onTime  Ponteiro para armazenar tempo total ligado (ms)
 * @param offTime Ponteiro para armazenar tempo total desligado (ms)
 * @param lastChange Ponteiro para armazenar timestamp da última mudança
 */
void getIgnicaoStatistics(unsigned long* onTime, unsigned long* offTime, unsigned long* lastChange);

/**
 * Reseta as estatísticas de uso da ignição.
 */
void resetIgnicaoStatistics();

/**
 * Obtém o estado bruto do pino de ignição (sem debounce).
 * Útil para debug ou diagnóstico.
 * 
 * @return Estado atual do pino (HIGH ou LOW)
 */
int getIgnicaoRawState();

/**
 * Callback opcional chamado quando o status da ignição muda.
 * Deve ser implementado pelo usuário se necessário.
 * 
 * @param newStatus Novo status da ignição (true = ON, false = OFF)
 */
extern void onIgnicaoStatusChange(bool newStatus);

#ifdef __cplusplus
}
#endif

#endif // IGNICAO_CONTROL_H