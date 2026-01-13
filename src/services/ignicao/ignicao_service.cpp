/**
 * ============================================================================
 * SERVICO DE IGNICAO - IMPLEMENTACAO
 * ============================================================================
 *
 * Servico refatorado para monitoramento de ignicao.
 * Thread-safe com mutex e callbacks.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "services/ignicao/ignicao_service.h"
#include "config/app_config.h"
#include "utils/time_utils.h"
#include "utils/debug_utils.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

LOG_TAG("IGNICAO_SVC");

// ============================================================================
// IMPLEMENTACAO DA CLASSE
// ============================================================================

IgnicaoService* IgnicaoService::instance = nullptr;

IgnicaoService::IgnicaoService()
    : status(false)
    , debounceOn(IGNICAO_DEBOUNCE_ON_S)
    , debounceOff(IGNICAO_DEBOUNCE_OFF_S)
    , mutex(nullptr)
    , taskHandle(nullptr)
    , callback(nullptr)
    , initialized(false)
    , running(false)
    , debounceInProgress(false)
    , lastPinState(false)
    , targetState(false)
    , debounceStartTime(0)
{
    memset(&stats, 0, sizeof(stats));
}

IgnicaoService::~IgnicaoService() {
    stop();
    if (mutex) {
        vSemaphoreDelete(mutex);
        mutex = nullptr;
    }
}

IgnicaoService* IgnicaoService::getInstance() {
    if (instance == nullptr) {
        instance = new IgnicaoService();
    }
    return instance;
}

void IgnicaoService::destroyInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

// ============================================================================
// INICIALIZACAO
// ============================================================================

bool IgnicaoService::init(float debounceOnSec, float debounceOffSec) {
    if (initialized) {
        LOG_W(TAG, "Servico ja inicializado");
        return true;
    }

    // Validar parametros
    if (debounceOnSec < IGNICAO_MIN_DEBOUNCE || debounceOnSec > IGNICAO_MAX_DEBOUNCE ||
        debounceOffSec < IGNICAO_MIN_DEBOUNCE || debounceOffSec > IGNICAO_MAX_DEBOUNCE) {
        LOG_E(TAG, "Parametros de debounce invalidos");
        return false;
    }

    debounceOn = debounceOnSec;
    debounceOff = debounceOffSec;

    // Criar mutex
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr) {
        LOG_E(TAG, "Falha ao criar mutex");
        return false;
    }

    // Configurar GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << IGNICAO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Ler estado inicial
    bool initialState = gpio_get_level((gpio_num_t)IGNICAO_PIN);
    status = initialState;
    lastPinState = initialState;

    // Inicializar estatisticas
    stats.sessionStartTime = time_millis();
    stats.lastChangeTime = time_millis();
    stats.totalOnTime = 0;
    stats.totalOffTime = 0;

    initialized = true;
    LOG_I(TAG, "Ignicao inicializada. Estado inicial: %s", initialState ? "ON" : "OFF");
    LOG_I(TAG, "Debounce ON: %.1fs, OFF: %.1fs", debounceOn, debounceOff);

    return true;
}

// ============================================================================
// GETTERS (THREAD-SAFE)
// ============================================================================

bool IgnicaoService::getStatus() const {
    bool currentStatus = false;

    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        currentStatus = status;
        xSemaphoreGive(mutex);
    }

    return currentStatus;
}

bool IgnicaoService::getRawStatus() const {
    return gpio_get_level((gpio_num_t)IGNICAO_PIN);
}

void IgnicaoService::setDebounce(float debounceOnSec, float debounceOffSec) {
    if (!initialized) return;

    if (debounceOnSec < IGNICAO_MIN_DEBOUNCE || debounceOnSec > IGNICAO_MAX_DEBOUNCE ||
        debounceOffSec < IGNICAO_MIN_DEBOUNCE || debounceOffSec > IGNICAO_MAX_DEBOUNCE) {
        LOG_W(TAG, "Valores de debounce invalidos, ignorando");
        return;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        debounceOn = debounceOnSec;
        debounceOff = debounceOffSec;
        debounceInProgress = false;  // Reset debounce em andamento
        xSemaphoreGive(mutex);

        LOG_I(TAG, "Debounce atualizado - ON: %.1fs, OFF: %.1fs", debounceOn, debounceOff);
    }
}

void IgnicaoService::getDebounce(float* debounceOnOut, float* debounceOffOut) const {
    if (!debounceOnOut || !debounceOffOut) return;

    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *debounceOnOut = debounceOn;
        *debounceOffOut = debounceOff;
        xSemaphoreGive(mutex);
    }
}

void IgnicaoService::setCallback(IgnicaoCallback cb) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        callback = cb;
        xSemaphoreGive(mutex);
    }
}

IgnicaoStats IgnicaoService::getStats() const {
    IgnicaoStats currentStats;
    memset(&currentStats, 0, sizeof(currentStats));

    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        currentStats = stats;

        // Adicionar tempo da sessao atual
        uint32_t currentTime = time_millis();
        uint32_t sessionDuration = currentTime - stats.lastChangeTime;

        if (status) {
            currentStats.totalOnTime += sessionDuration;
        } else {
            currentStats.totalOffTime += sessionDuration;
        }

        xSemaphoreGive(mutex);
    }

    return currentStats;
}

void IgnicaoService::resetStats() {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        stats.totalOnTime = 0;
        stats.totalOffTime = 0;
        stats.lastChangeTime = time_millis();
        stats.sessionStartTime = time_millis();
        xSemaphoreGive(mutex);

        LOG_I(TAG, "Estatisticas resetadas");
    }
}

// ============================================================================
// CONTROLE DE TASK
// ============================================================================

void IgnicaoService::start() {
    if (!initialized) {
        LOG_E(TAG, "Servico nao inicializado");
        return;
    }

    if (running) {
        LOG_W(TAG, "Monitoramento ja em execucao");
        return;
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        monitorTask,
        "IgnicaoMonitor",
        IGNICAO_TASK_STACK_SIZE,
        this,
        IGNICAO_TASK_PRIORITY,
        &taskHandle,
        IGNICAO_TASK_CORE
    );

    if (result != pdPASS) {
        LOG_E(TAG, "Falha ao criar task de monitoramento");
        return;
    }

    running = true;
    LOG_I(TAG, "Monitoramento iniciado");
}

void IgnicaoService::stop() {
    if (!running || !taskHandle) {
        return;
    }

    running = false;
    vTaskDelay(pdMS_TO_TICKS(IGNICAO_CHECK_INTERVAL + 50));

    if (taskHandle) {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }

    LOG_I(TAG, "Monitoramento parado");
}

bool IgnicaoService::isRunning() const {
    return running;
}

// ============================================================================
// TASK DE MONITORAMENTO
// ============================================================================

void IgnicaoService::monitorTask(void* arg) {
    IgnicaoService* self = static_cast<IgnicaoService*>(arg);
    if (!self) {
        vTaskDelete(nullptr);
        return;
    }

    LOG_I(TAG, "Task de monitoramento iniciada no Core %d", xPortGetCoreID());

    while (self->running) {
        self->processDebounce();
        vTaskDelay(pdMS_TO_TICKS(IGNICAO_CHECK_INTERVAL));
    }

    vTaskDelete(nullptr);
}

void IgnicaoService::processDebounce() {
    bool currentPinState = gpio_get_level((gpio_num_t)IGNICAO_PIN);

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }

    uint32_t currentTime = time_millis();

    // Detectar mudanca no pino
    if (currentPinState != lastPinState) {
        lastPinState = currentPinState;
        debounceStartTime = currentTime;
        debounceInProgress = true;
        targetState = currentPinState;
    }

    // Processar debounce em andamento
    if (debounceInProgress) {
        float requiredDebounce = targetState ? debounceOn : debounceOff;
        uint32_t debounceMs = (uint32_t)(requiredDebounce * 1000);

        if (requiredDebounce == 0 || (currentTime - debounceStartTime) >= debounceMs) {
            // Confirmar estado
            bool confirmState = gpio_get_level((gpio_num_t)IGNICAO_PIN);

            if (confirmState == targetState && status != targetState) {
                // Atualizar estatisticas
                uint32_t duration = currentTime - stats.lastChangeTime;
                if (status) {
                    stats.totalOnTime += duration;
                } else {
                    stats.totalOffTime += duration;
                }

                // Atualizar estado
                status = targetState;
                stats.lastChangeTime = currentTime;
                debounceInProgress = false;

                LOG_I(TAG, "Ignicao mudou para: %s", targetState ? "ON" : "OFF");

                // Callback (fora do mutex)
                IgnicaoCallback cb = callback;
                xSemaphoreGive(mutex);

                if (cb) {
                    cb(targetState);
                }

                return;  // Ja liberou o mutex
            } else if (confirmState != targetState) {
                // Estado nao confirmado
                debounceInProgress = false;
                lastPinState = confirmState;
            } else {
                debounceInProgress = false;
            }
        }
    }

    xSemaphoreGive(mutex);
}

// ============================================================================
// INTERFACE C
// ============================================================================

static IgnicaoService* g_ignicaoService = nullptr;

extern "C" {

bool ignicao_init(float debounceOn, float debounceOff) {
    g_ignicaoService = IgnicaoService::getInstance();
    return g_ignicaoService->init(debounceOn, debounceOff);
}

bool ignicao_get_status(void) {
    if (g_ignicaoService) {
        return g_ignicaoService->getStatus();
    }
    return false;
}

int ignicao_get_raw_status(void) {
    if (g_ignicaoService) {
        return g_ignicaoService->getRawStatus() ? 1 : 0;
    }
    return 0;
}

void ignicao_set_debounce(float debounceOn, float debounceOff) {
    if (g_ignicaoService) {
        g_ignicaoService->setDebounce(debounceOn, debounceOff);
    }
}

void ignicao_start(void) {
    if (g_ignicaoService) {
        g_ignicaoService->start();
    }
}

void ignicao_stop(void) {
    if (g_ignicaoService) {
        g_ignicaoService->stop();
    }
}

} // extern "C"
