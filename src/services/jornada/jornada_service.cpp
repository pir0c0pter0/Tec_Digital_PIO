/**
 * ============================================================================
 * SERVICO DE JORNADA - IMPLEMENTACAO
 * ============================================================================
 *
 * Servico refatorado para gerenciamento de jornada de motoristas.
 * Thread-safe com mutex e callbacks.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "services/jornada/jornada_service.h"
#include "config/app_config.h"
#include "utils/time_utils.h"
#include "utils/debug_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

LOG_TAG("JORNADA_SVC");

// ============================================================================
// NOMES DOS ESTADOS
// ============================================================================

static const char* ESTADO_NOMES[] = {
    "Inativo",
    "Jornada",
    "Manobra",
    "Refeicao",
    "Espera",
    "Descarga",
    "Abastecimento"
};

// ============================================================================
// IMPLEMENTACAO DA CLASSE
// ============================================================================

JornadaService* JornadaService::instance = nullptr;

JornadaService::JornadaService()
    : mutex(nullptr)
    , callback(nullptr)
    , initialized(false)
{
    memset(motoristas, 0, sizeof(motoristas));
}

JornadaService::~JornadaService() {
    if (mutex) {
        vSemaphoreDelete(mutex);
        mutex = nullptr;
    }
}

JornadaService* JornadaService::getInstance() {
    if (instance == nullptr) {
        instance = new JornadaService();
    }
    return instance;
}

void JornadaService::destroyInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

// ============================================================================
// INICIALIZACAO
// ============================================================================

void JornadaService::init() {
    if (initialized) {
        LOG_W(TAG, "Servico ja inicializado");
        return;
    }

    // Criar mutex
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr) {
        LOG_E(TAG, "Falha ao criar mutex");
        return;
    }

    // Inicializar motoristas
    for (int i = 0; i < MAX_MOTORISTAS; i++) {
        memset(&motoristas[i], 0, sizeof(DadosMotorista));
        motoristas[i].id = 0;
        motoristas[i].estadoAtual = EstadoJornada::INATIVO;
        motoristas[i].ativo = false;
    }

    initialized = true;
    LOG_I(TAG, "Servico de jornada inicializado (max %d motoristas)", MAX_MOTORISTAS);
}

// ============================================================================
// GESTAO DE MOTORISTAS
// ============================================================================

bool JornadaService::addMotorista(int id, const char* nome) {
    if (!initialized || !nome) {
        return false;
    }

    if (id < 1 || id > MAX_MOTORISTAS) {
        LOG_W(TAG, "ID de motorista invalido: %d", id);
        return false;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    // Verificar se ja existe
    int existingIdx = findMotoristaIndex(id);
    if (existingIdx >= 0) {
        xSemaphoreGive(mutex);
        LOG_W(TAG, "Motorista %d ja existe", id);
        return false;
    }

    // Encontrar slot vazio
    for (int i = 0; i < MAX_MOTORISTAS; i++) {
        if (!motoristas[i].ativo) {
            motoristas[i].id = id;
            strncpy(motoristas[i].nome, nome, MAX_NOME_MOTORISTA - 1);
            motoristas[i].nome[MAX_NOME_MOTORISTA - 1] = '\0';
            motoristas[i].estadoAtual = EstadoJornada::INATIVO;
            motoristas[i].tempoInicio = 0;
            motoristas[i].tempoTotalJornada = 0;
            motoristas[i].tempoTotalManobra = 0;
            motoristas[i].tempoTotalRefeicao = 0;
            motoristas[i].tempoTotalEspera = 0;
            motoristas[i].tempoTotalDescarga = 0;
            motoristas[i].tempoTotalAbastecimento = 0;
            motoristas[i].ativo = true;

            JornadaCallback cb = callback;
            xSemaphoreGive(mutex);

            LOG_I(TAG, "Motorista adicionado: ID=%d, Nome=%s", id, nome);

            if (cb) {
                cb(id, EstadoJornada::INATIVO);
            }

            return true;
        }
    }

    xSemaphoreGive(mutex);
    LOG_W(TAG, "Sem slots disponiveis para motorista");
    return false;
}

void JornadaService::removeMotorista(int id) {
    if (!initialized) return;

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    int idx = findMotoristaIndex(id);
    if (idx >= 0) {
        motoristas[idx].ativo = false;
        motoristas[idx].id = 0;

        JornadaCallback cb = callback;
        xSemaphoreGive(mutex);

        LOG_I(TAG, "Motorista %d removido", id);

        if (cb) {
            cb(id, EstadoJornada::INATIVO);
        }

        return;
    }

    xSemaphoreGive(mutex);
}

bool JornadaService::getMotorista(int id, DadosMotorista* dados) const {
    if (!initialized || !dados) {
        return false;
    }

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    int idx = findMotoristaIndex(id);
    if (idx >= 0) {
        // Copia thread-safe
        memcpy(dados, &motoristas[idx], sizeof(DadosMotorista));
        xSemaphoreGive(mutex);
        return true;
    }

    xSemaphoreGive(mutex);
    return false;
}

int JornadaService::getNumMotoristasAtivos() const {
    if (!initialized) return 0;

    int count = 0;

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < MAX_MOTORISTAS; i++) {
            if (motoristas[i].ativo) {
                count++;
            }
        }
        xSemaphoreGive(mutex);
    }

    return count;
}

// ============================================================================
// GESTAO DE ESTADOS
// ============================================================================

bool JornadaService::iniciarEstado(int id, EstadoJornada estado) {
    if (!initialized) return false;

    if (estado == EstadoJornada::INATIVO) {
        return finalizarEstado(id);
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    int idx = findMotoristaIndex(id);
    if (idx < 0) {
        xSemaphoreGive(mutex);
        return false;
    }

    // Atualizar tempo do estado anterior
    if (motoristas[idx].estadoAtual != EstadoJornada::INATIVO) {
        atualizarTempoAcumulado(idx);
    }

    // Definir novo estado
    motoristas[idx].estadoAtual = estado;
    motoristas[idx].tempoInicio = time_millis();

    JornadaCallback cb = callback;
    xSemaphoreGive(mutex);

    LOG_I(TAG, "Motorista %d iniciou estado: %s", id, getNomeEstado(estado));

    if (cb) {
        cb(id, estado);
    }

    return true;
}

bool JornadaService::finalizarEstado(int id) {
    if (!initialized) return false;

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    int idx = findMotoristaIndex(id);
    if (idx < 0) {
        xSemaphoreGive(mutex);
        return false;
    }

    if (motoristas[idx].estadoAtual == EstadoJornada::INATIVO) {
        xSemaphoreGive(mutex);
        return false;
    }

    // Atualizar tempo acumulado
    atualizarTempoAcumulado(idx);

    EstadoJornada estadoAnterior = motoristas[idx].estadoAtual;
    motoristas[idx].estadoAtual = EstadoJornada::INATIVO;
    motoristas[idx].tempoInicio = 0;

    JornadaCallback cb = callback;
    xSemaphoreGive(mutex);

    LOG_I(TAG, "Motorista %d finalizou estado: %s", id, getNomeEstado(estadoAnterior));

    if (cb) {
        cb(id, EstadoJornada::INATIVO);
    }

    return true;
}

// ============================================================================
// VERIFICACOES
// ============================================================================

bool JornadaService::temJornadaAtiva() const {
    if (!initialized) return false;

    bool ativo = false;

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < MAX_MOTORISTAS; i++) {
            if (motoristas[i].ativo && motoristas[i].estadoAtual == EstadoJornada::JORNADA) {
                ativo = true;
                break;
            }
        }
        xSemaphoreGive(mutex);
    }

    return ativo;
}

bool JornadaService::temEstadoPausadoAtivo() const {
    if (!initialized) return false;

    bool ativo = false;

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < MAX_MOTORISTAS; i++) {
            if (motoristas[i].ativo) {
                EstadoJornada e = motoristas[i].estadoAtual;
                if (e == EstadoJornada::REFEICAO || e == EstadoJornada::ESPERA ||
                    e == EstadoJornada::DESCARGA || e == EstadoJornada::ABASTECIMENTO) {
                    ativo = true;
                    break;
                }
            }
        }
        xSemaphoreGive(mutex);
    }

    return ativo;
}

const char* JornadaService::getNomeEstado(EstadoJornada estado) const {
    int idx = static_cast<int>(estado);
    if (idx >= 0 && idx < static_cast<int>(EstadoJornada::MAX_ESTADOS)) {
        return ESTADO_NOMES[idx];
    }
    return "Desconhecido";
}

uint32_t JornadaService::getTempoEstadoAtual(int id) const {
    if (!initialized) return 0;

    uint32_t tempo = 0;

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int idx = findMotoristaIndex(id);
        if (idx >= 0 && motoristas[idx].estadoAtual != EstadoJornada::INATIVO) {
            tempo = time_millis() - motoristas[idx].tempoInicio;
        }
        xSemaphoreGive(mutex);
    }

    return tempo;
}

void JornadaService::setCallback(JornadaCallback cb) {
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        callback = cb;
        xSemaphoreGive(mutex);
    }
}

// ============================================================================
// METODOS PRIVADOS
// ============================================================================

int JornadaService::findMotoristaIndex(int id) const {
    for (int i = 0; i < MAX_MOTORISTAS; i++) {
        if (motoristas[i].ativo && motoristas[i].id == id) {
            return i;
        }
    }
    return -1;
}

void JornadaService::atualizarTempoAcumulado(int idx) {
    if (idx < 0 || idx >= MAX_MOTORISTAS) return;

    DadosMotorista& m = motoristas[idx];
    if (m.estadoAtual == EstadoJornada::INATIVO) return;

    uint32_t duracao = time_millis() - m.tempoInicio;

    switch (m.estadoAtual) {
        case EstadoJornada::JORNADA:
            m.tempoTotalJornada += duracao;
            break;
        case EstadoJornada::MANOBRA:
            m.tempoTotalManobra += duracao;
            break;
        case EstadoJornada::REFEICAO:
            m.tempoTotalRefeicao += duracao;
            break;
        case EstadoJornada::ESPERA:
            m.tempoTotalEspera += duracao;
            break;
        case EstadoJornada::DESCARGA:
            m.tempoTotalDescarga += duracao;
            break;
        case EstadoJornada::ABASTECIMENTO:
            m.tempoTotalAbastecimento += duracao;
            break;
        default:
            break;
    }
}

// ============================================================================
// INTERFACE C
// ============================================================================

static JornadaService* g_jornadaService = nullptr;

extern "C" {

void jornada_init(void) {
    g_jornadaService = JornadaService::getInstance();
    g_jornadaService->init();
}

bool jornada_add_motorista(int id, const char* nome) {
    if (g_jornadaService) {
        return g_jornadaService->addMotorista(id, nome);
    }
    return false;
}

void jornada_remove_motorista(int id) {
    if (g_jornadaService) {
        g_jornadaService->removeMotorista(id);
    }
}

bool jornada_iniciar_estado(int id, estado_jornada_t estado) {
    if (g_jornadaService) {
        return g_jornadaService->iniciarEstado(id, static_cast<EstadoJornada>(estado));
    }
    return false;
}

bool jornada_finalizar_estado(int id) {
    if (g_jornadaService) {
        return g_jornadaService->finalizarEstado(id);
    }
    return false;
}

bool jornada_tem_ativa(void) {
    if (g_jornadaService) {
        return g_jornadaService->temJornadaAtiva();
    }
    return false;
}

const char* jornada_get_nome_estado(estado_jornada_t estado) {
    if (g_jornadaService) {
        return g_jornadaService->getNomeEstado(static_cast<EstadoJornada>(estado));
    }
    return "Desconhecido";
}

} // extern "C"
