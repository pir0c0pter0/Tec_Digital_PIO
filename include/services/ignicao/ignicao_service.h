/**
 * ============================================================================
 * SERVICO DE IGNICAO - HEADER
 * ============================================================================
 *
 * Servico refatorado para monitoramento de ignicao.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef IGNICAO_SERVICE_H
#define IGNICAO_SERVICE_H

#include "interfaces/i_ignicao.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef __cplusplus

/**
 * Implementacao do servico de ignicao
 */
class IgnicaoService : public IIgnicaoService {
public:
    // Singleton
    static IgnicaoService* getInstance();
    static void destroyInstance();

    // IIgnicaoService interface
    bool init(float debounceOn, float debounceOff) override;
    bool getStatus() const override;
    bool getRawStatus() const override;
    void setDebounce(float debounceOn, float debounceOff) override;
    void getDebounce(float* debounceOn, float* debounceOff) const override;
    void setCallback(IgnicaoCallback callback) override;
    IgnicaoStats getStats() const override;
    void resetStats() override;
    void start() override;
    void stop() override;
    bool isRunning() const override;

private:
    IgnicaoService();
    ~IgnicaoService();

    // Nao permitir copia
    IgnicaoService(const IgnicaoService&) = delete;
    IgnicaoService& operator=(const IgnicaoService&) = delete;

    // Task de monitoramento
    static void monitorTask(void* arg);
    void processDebounce();

    // Singleton
    static IgnicaoService* instance;

    // Estado
    volatile bool status;
    float debounceOn;
    float debounceOff;

    // Sincronizacao
    SemaphoreHandle_t mutex;
    TaskHandle_t taskHandle;

    // Callback
    IgnicaoCallback callback;

    // Flags
    bool initialized;
    volatile bool running;

    // Debounce interno
    bool debounceInProgress;
    bool lastPinState;
    bool targetState;
    uint32_t debounceStartTime;

    // Estatisticas
    IgnicaoStats stats;
};

#endif // __cplusplus

#endif // IGNICAO_SERVICE_H
