/**
 * ============================================================================
 * INTERFACE DE NVS (Non-Volatile Storage)
 * ============================================================================
 *
 * Abstrai o gerenciamento de persistencia de dados em NVS.
 * Permite trocar a implementacao sem afetar o resto do sistema.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef I_NVS_H
#define I_NVS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus

/**
 * Interface abstrata para gerenciamento de NVS
 */
class INvsManager {
public:
    virtual ~INvsManager() = default;

    /**
     * Inicializa a particao NVS
     * @return true se inicializado com sucesso
     */
    virtual bool init() = 0;

    // ========================================================================
    // Configuracoes (volume, brilho)
    // ========================================================================

    /**
     * Salva volume no NVS
     * @param volume Valor de 0 a 21
     * @return true se salvo com sucesso
     */
    virtual bool saveVolume(uint8_t volume) = 0;

    /**
     * Carrega volume do NVS
     * @param defaultVal Valor padrao se nao encontrado
     * @return Volume salvo ou defaultVal
     */
    virtual uint8_t loadVolume(uint8_t defaultVal) = 0;

    /**
     * Salva brilho no NVS
     * @param brightness Valor de 0 a 100
     * @return true se salvo com sucesso
     */
    virtual bool saveBrightness(uint8_t brightness) = 0;

    /**
     * Carrega brilho do NVS
     * @param defaultVal Valor padrao se nao encontrado
     * @return Brilho salvo ou defaultVal
     */
    virtual uint8_t loadBrightness(uint8_t defaultVal) = 0;

    // ========================================================================
    // Estado de jornada por motorista
    // ========================================================================

    /**
     * Salva estado de jornada de um motorista
     * @param motoristId ID do motorista (0-2)
     * @param state Ponteiro para struct de estado
     * @param size Tamanho da struct
     * @return true se salvo com sucesso
     */
    virtual bool saveJornadaState(uint8_t motoristId, const void* state, size_t size) = 0;

    /**
     * Carrega estado de jornada de um motorista
     * @param motoristId ID do motorista (0-2)
     * @param state Ponteiro para receber struct de estado
     * @param size Tamanho esperado da struct
     * @return true se carregado com sucesso
     */
    virtual bool loadJornadaState(uint8_t motoristId, void* state, size_t size) = 0;

    /**
     * Limpa estado de jornada de um motorista
     * @param motoristId ID do motorista (0-2)
     * @return true se limpo com sucesso
     */
    virtual bool clearJornadaState(uint8_t motoristId) = 0;
};

extern "C" {
#endif

// Interface C para compatibilidade
bool nvs_manager_init(void);
bool nvs_manager_save_volume(uint8_t volume);
uint8_t nvs_manager_load_volume(uint8_t default_val);
bool nvs_manager_save_brightness(uint8_t brightness);
uint8_t nvs_manager_load_brightness(uint8_t default_val);

#ifdef __cplusplus
}
#endif

#endif // I_NVS_H
