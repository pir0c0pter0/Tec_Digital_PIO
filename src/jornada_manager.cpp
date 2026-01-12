/**
 * ============================================================================
 * GERENCIADOR DE JORNADA - IMPLEMENTAÇÃO
 * ============================================================================
 */

#include "jornada_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "JORNADA_MGR";

// Macros de compatibilidade
#define millis() ((uint32_t)(esp_timer_get_time() / 1000ULL))
#define Serial_println(x) ESP_LOGI(TAG, "%s", x)
#define Serial_printf(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

static Motorista motoristas[MAX_MOTORISTAS];
static SemaphoreHandle_t jornada_mutex = NULL;

// ============================================================================
// FUNÇÕES PRIVADAS
// ============================================================================

/**
 * Encontra índice do motorista pelo ID
 */
static int findMotoristaIndex(int id) {
    for (int i = 0; i < MAX_MOTORISTAS; i++) {
        if (motoristas[i].ativo && motoristas[i].id == id) {
            return i;
        }
    }
    return -1;
}

/**
 * Atualiza o tempo acumulado do estado anterior
 */
static void atualizarTempoAcumulado(Motorista* m) {
    if (m->estadoAtual == ESTADO_INATIVO) return;
    
    unsigned long duracao = millis() - m->tempoInicio;
    
    switch (m->estadoAtual) {
        case ESTADO_JORNADA:
            m->tempoTotalJornada += duracao;
            break;
        case ESTADO_MANOBRA:
            m->tempoTotalManobra += duracao;
            break;
        case ESTADO_REFEICAO:
            m->tempoTotalRefeicao += duracao;
            break;
        case ESTADO_ESPERA:
            m->tempoTotalEspera += duracao;
            break;
        case ESTADO_DESCARGA:
            m->tempoTotalDescarga += duracao;
            break;
        case ESTADO_ABASTECIMENTO:
            m->tempoTotalAbastecimento += duracao;
            break;
        default:
            break;
    }
}

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

/**
 * Inicializa o sistema de gerenciamento de jornada
 */
void initJornadaManager() {
    // Cria mutex se não existir
    if (jornada_mutex == NULL) {
        jornada_mutex = xSemaphoreCreateMutex();
    }
    
    // Inicializa array de motoristas
    for (int i = 0; i < MAX_MOTORISTAS; i++) {
        motoristas[i].id = 0;
        motoristas[i].nome[0] = '\0';
        motoristas[i].estadoAtual = ESTADO_INATIVO;
        motoristas[i].tempoInicio = 0;
        motoristas[i].tempoTotalJornada = 0;
        motoristas[i].tempoTotalManobra = 0;
        motoristas[i].tempoTotalRefeicao = 0;
        motoristas[i].tempoTotalEspera = 0;
        motoristas[i].tempoTotalDescarga = 0;
        motoristas[i].tempoTotalAbastecimento = 0;
        motoristas[i].ativo = false;
    }
    
    ESP_LOGI(TAG, "Sistema de jornada inicializado");
}

/**
 * Adiciona um motorista ao sistema
 */
bool addMotorista(int id, const char* nome) {
    if (id < 1 || id > MAX_MOTORISTAS || nome == NULL) {
        return false;
    }
    
    if (xSemaphoreTake(jornada_mutex, portMAX_DELAY) == pdTRUE) {
        // Verifica se já existe
        int idx = findMotoristaIndex(id);
        if (idx >= 0) {
            xSemaphoreGive(jornada_mutex);
            return false; // Já existe
        }
        
        // Encontra slot vazio
        for (int i = 0; i < MAX_MOTORISTAS; i++) {
            if (!motoristas[i].ativo) {
                motoristas[i].id = id;
                strncpy(motoristas[i].nome, nome, sizeof(motoristas[i].nome) - 1);
                motoristas[i].nome[sizeof(motoristas[i].nome) - 1] = '\0';
                motoristas[i].estadoAtual = ESTADO_INATIVO;
                motoristas[i].tempoInicio = 0;
                motoristas[i].tempoTotalJornada = 0;
                motoristas[i].tempoTotalManobra = 0;
                motoristas[i].tempoTotalRefeicao = 0;
                motoristas[i].tempoTotalEspera = 0;
                motoristas[i].tempoTotalDescarga = 0;
                motoristas[i].tempoTotalAbastecimento = 0;
                motoristas[i].ativo = true;
                
                xSemaphoreGive(jornada_mutex);
                
                ESP_LOGI(TAG, "Motorista %d (%s) adicionado\n", id, nome);
                onJornadaStateChange();
                return true;
            }
        }
        
        xSemaphoreGive(jornada_mutex);
    }
    
    return false; // Sem slots disponíveis
}

/**
 * Remove um motorista do sistema
 */
void removeMotorista(int id) {
    if (xSemaphoreTake(jornada_mutex, portMAX_DELAY) == pdTRUE) {
        int idx = findMotoristaIndex(id);
        if (idx >= 0) {
            motoristas[idx].ativo = false;
            motoristas[idx].id = 0;
            ESP_LOGI(TAG, "Motorista %d removido\n", id);
            onJornadaStateChange();
        }
        xSemaphoreGive(jornada_mutex);
    }
}

/**
 * Obtém ponteiro para os dados de um motorista
 */
Motorista* getMotorista(int id) {
    int idx = findMotoristaIndex(id);
    if (idx >= 0) {
        return &motoristas[idx];
    }
    return NULL;
}

/**
 * Obtém o número de motoristas ativos
 */
int getNumMotoristasAtivos() {
    int count = 0;
    if (xSemaphoreTake(jornada_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_MOTORISTAS; i++) {
            if (motoristas[i].ativo) {
                count++;
            }
        }
        xSemaphoreGive(jornada_mutex);
    }
    return count;
}

/**
 * Inicia um estado para um motorista
 */
bool iniciarEstado(int id, EstadoJornada estado) {
    if (estado == ESTADO_INATIVO) {
        return false;
    }
    
    if (xSemaphoreTake(jornada_mutex, portMAX_DELAY) == pdTRUE) {
        int idx = findMotoristaIndex(id);
        if (idx >= 0) {
            Motorista* m = &motoristas[idx];
            
            // Atualiza tempo acumulado do estado anterior
            if (m->estadoAtual != ESTADO_INATIVO) {
                atualizarTempoAcumulado(m);
            }
            
            // Define novo estado
            m->estadoAtual = estado;
            m->tempoInicio = millis();
            
            xSemaphoreGive(jornada_mutex);
            
            ESP_LOGI(TAG, "Motorista %d iniciou estado: %s\n", id, getNomeEstado(estado));
            onJornadaStateChange();
            return true;
        }
        xSemaphoreGive(jornada_mutex);
    }
    
    return false;
}

/**
 * Finaliza o estado atual de um motorista
 */
bool finalizarEstado(int id) {
    if (xSemaphoreTake(jornada_mutex, portMAX_DELAY) == pdTRUE) {
        int idx = findMotoristaIndex(id);
        if (idx >= 0) {
            Motorista* m = &motoristas[idx];
            
            if (m->estadoAtual != ESTADO_INATIVO) {
                // Atualiza tempo acumulado
                atualizarTempoAcumulado(m);
                
                EstadoJornada estadoAnterior = m->estadoAtual;
                m->estadoAtual = ESTADO_INATIVO;
                m->tempoInicio = 0;
                
                xSemaphoreGive(jornada_mutex);
                
                ESP_LOGI(TAG, "Motorista %d finalizou estado: %s\n", id, getNomeEstado(estadoAnterior));
                onJornadaStateChange();
                return true;
            }
        }
        xSemaphoreGive(jornada_mutex);
    }
    
    return false;
}

/**
 * Verifica se algum motorista tem jornada ativa
 */
bool temJornadaAtiva() {
    bool ativo = false;
    if (xSemaphoreTake(jornada_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_MOTORISTAS; i++) {
            if (motoristas[i].ativo && motoristas[i].estadoAtual == ESTADO_JORNADA) {
                ativo = true;
                break;
            }
        }
        xSemaphoreGive(jornada_mutex);
    }
    return ativo;
}

/**
 * Verifica se algum motorista tem estado pausado ativo
 */
bool temEstadoPausadoAtivo() {
    bool ativo = false;
    if (xSemaphoreTake(jornada_mutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_MOTORISTAS; i++) {
            if (motoristas[i].ativo) {
                EstadoJornada e = motoristas[i].estadoAtual;
                if (e == ESTADO_REFEICAO || e == ESTADO_ESPERA || 
                    e == ESTADO_DESCARGA || e == ESTADO_ABASTECIMENTO) {
                    ativo = true;
                    break;
                }
            }
        }
        xSemaphoreGive(jornada_mutex);
    }
    return ativo;
}

/**
 * Obtém o nome do estado
 */
const char* getNomeEstado(EstadoJornada estado) {
    switch (estado) {
        case ESTADO_INATIVO: return "Inativo";
        case ESTADO_JORNADA: return "Jornada";
        case ESTADO_MANOBRA: return "Manobra";
        case ESTADO_REFEICAO: return "Refeição";
        case ESTADO_ESPERA: return "Espera";
        case ESTADO_DESCARGA: return "Descarga";
        case ESTADO_ABASTECIMENTO: return "Abastecimento";
        default: return "Desconhecido";
    }
}

/**
 * Obtém estatísticas de um motorista
 */
bool getEstatisticas(int id, unsigned long* tempoAtual) {
    if (xSemaphoreTake(jornada_mutex, portMAX_DELAY) == pdTRUE) {
        int idx = findMotoristaIndex(id);
        if (idx >= 0) {
            Motorista* m = &motoristas[idx];
            
            if (tempoAtual != NULL && m->estadoAtual != ESTADO_INATIVO) {
                *tempoAtual = millis() - m->tempoInicio;
            }
            
            xSemaphoreGive(jornada_mutex);
            return true;
        }
        xSemaphoreGive(jornada_mutex);
    }
    return false;
}

// ============================================================================
// FIM DA IMPLEMENTAÇÃO
// ============================================================================

