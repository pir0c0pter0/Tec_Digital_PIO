/**
 * ============================================================================
 * NVS MANAGER - HEADER
 * ============================================================================
 *
 * Gerenciamento de persistencia em NVS usando particao dedicada (nvs_data).
 * Persiste configuracoes (volume, brilho) e estado de jornada por motorista.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include "interfaces/i_nvs.h"
#include "config/app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"

#ifdef __cplusplus

/**
 * Struct compacta para persistencia de estado de jornada por motorista.
 * Usa packed attribute para garantir layout binario determinístico no NVS.
 * Campo version permite migracao futura sem perda de dados.
 */
struct __attribute__((packed)) NvsJornadaState {
    uint8_t version;                    // NVS_JORNADA_VERSION para migracao
    uint8_t estadoAtual;                // EstadoJornada enum value (0-6)
    uint32_t tempoInicio;               // millis() snapshot quando estado iniciou
    uint32_t tempoTotalJornada;         // Tempo acumulado em JORNADA (ms)
    uint32_t tempoTotalManobra;         // Tempo acumulado em MANOBRA (ms)
    uint32_t tempoTotalRefeicao;        // Tempo acumulado em REFEICAO (ms)
    uint32_t tempoTotalEspera;          // Tempo acumulado em ESPERA (ms)
    uint32_t tempoTotalDescarga;        // Tempo acumulado em DESCARGA (ms)
    uint32_t tempoTotalAbastecimento;   // Tempo acumulado em ABASTECIMENTO (ms)
    uint8_t ativo;                      // Slot ativo (1) ou inativo (0)
};

/**
 * Implementacao do gerenciador de NVS usando particao dedicada.
 *
 * Usa NVS_PARTITION_LABEL ("nvs_data") separado do NVS do sistema.
 * Namespaces: "settings" para volume/brilho, "jornada" para estado por motorista.
 * Thread-safe via mutex FreeRTOS.
 */
class NvsManager : public INvsManager {
public:
    // Singleton
    static NvsManager* getInstance();

    // INvsManager interface
    bool init() override;
    bool saveVolume(uint8_t volume) override;
    uint8_t loadVolume(uint8_t defaultVal) override;
    bool saveBrightness(uint8_t brightness) override;
    uint8_t loadBrightness(uint8_t defaultVal) override;
    bool saveJornadaState(uint8_t motoristId, const void* state, size_t size) override;
    bool loadJornadaState(uint8_t motoristId, void* state, size_t size) override;
    bool clearJornadaState(uint8_t motoristId) override;

private:
    NvsManager();
    ~NvsManager();

    // Nao permitir copia
    NvsManager(const NvsManager&) = delete;
    NvsManager& operator=(const NvsManager&) = delete;

    /**
     * Abre handle NVS na particao dedicada
     * @param ns Namespace (max 15 chars)
     * @param mode Modo de abertura (RO ou RW)
     * @param handle Ponteiro para receber o handle
     * @return true se aberto com sucesso
     */
    bool openHandle(const char* ns, nvs_open_mode_t mode, nvs_handle_t* handle);

    // Singleton
    static NvsManager* instance_;

    // Sincronizacao
    SemaphoreHandle_t mutex_;

    // Estado
    bool initialized_;
};

#endif // __cplusplus

#endif // NVS_MANAGER_H
