/**
 * ============================================================================
 * SERVICO DE JORNADA - HEADER
 * ============================================================================
 *
 * Servico refatorado para gerenciamento de jornada de motoristas.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef JORNADA_SERVICE_H
#define JORNADA_SERVICE_H

#include "config/app_config.h"
#include "interfaces/i_jornada.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus

/**
 * Implementacao do servico de jornada
 */
class JornadaService : public IJornadaService {
public:
    // Singleton
    static JornadaService* getInstance();
    static void destroyInstance();

    // IJornadaService interface
    void init() override;
    bool addMotorista(int id, const char* nome) override;
    void removeMotorista(int id) override;
    bool getMotorista(int id, DadosMotorista* dados) const override;
    int getNumMotoristasAtivos() const override;
    bool iniciarEstado(int id, EstadoJornada estado) override;
    bool finalizarEstado(int id) override;
    bool temJornadaAtiva() const override;
    bool temEstadoPausadoAtivo() const override;
    const char* getNomeEstado(EstadoJornada estado) const override;
    uint32_t getTempoEstadoAtual(int id) const override;
    void setCallback(JornadaCallback callback) override;

private:
    JornadaService();
    ~JornadaService();

    // Nao permitir copia
    JornadaService(const JornadaService&) = delete;
    JornadaService& operator=(const JornadaService&) = delete;

    // Metodos privados
    int findMotoristaIndex(int id) const;
    void atualizarTempoAcumulado(int idx);

    // Singleton
    static JornadaService* instance;

    // Dados
    DadosMotorista motoristas[MAX_MOTORISTAS];

    // Sincronizacao
    mutable SemaphoreHandle_t mutex;

    // Callback
    JornadaCallback callback;

    // Flags
    bool initialized;
};

#endif // __cplusplus

#endif // JORNADA_SERVICE_H
