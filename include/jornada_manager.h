/**
 * ============================================================================
 * GERENCIADOR DE JORNADA - HEADER
 * ============================================================================
 * 
 * Sistema de controle de jornada para até 3 motoristas simultâneos.
 * Gerencia estados de: jornada (direção), manobra, refeição, espera, 
 * descarga e abastecimento.
 * 
 * ============================================================================
 */

#ifndef JORNADA_MANAGER_H
#define JORNADA_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// ============================================================================
// DEFINIÇÕES
// ============================================================================

#define MAX_MOTORISTAS 3

// Estados possíveis de cada motorista
enum EstadoJornada {
    ESTADO_INATIVO = 0,
    ESTADO_JORNADA,      // Direção
    ESTADO_MANOBRA,
    ESTADO_REFEICAO,
    ESTADO_ESPERA,
    ESTADO_DESCARGA,
    ESTADO_ABASTECIMENTO
};

// Estrutura de dados de um motorista
struct Motorista {
    int id;                           // ID do motorista (1, 2 ou 3)
    char nome[32];                    // Nome do motorista
    EstadoJornada estadoAtual;        // Estado atual
    unsigned long tempoInicio;        // Timestamp do início do estado atual
    unsigned long tempoTotalJornada;  // Tempo total em jornada (direção)
    unsigned long tempoTotalManobra;  // Tempo total em manobra
    unsigned long tempoTotalRefeicao; // Tempo total em refeição
    unsigned long tempoTotalEspera;   // Tempo total em espera
    unsigned long tempoTotalDescarga; // Tempo total em descarga
    unsigned long tempoTotalAbastecimento; // Tempo total em abastecimento
    bool ativo;                       // Se o motorista está ativo no sistema
};

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

/**
 * Inicializa o sistema de gerenciamento de jornada
 */
void initJornadaManager();

/**
 * Adiciona um motorista ao sistema
 * @param id ID do motorista (1-3)
 * @param nome Nome do motorista
 * @return true se adicionado com sucesso
 */
bool addMotorista(int id, const char* nome);

/**
 * Remove um motorista do sistema
 * @param id ID do motorista
 */
void removeMotorista(int id);

/**
 * Obtém ponteiro para os dados de um motorista
 * @param id ID do motorista
 * @return Ponteiro para struct Motorista ou NULL se não encontrado
 */
Motorista* getMotorista(int id);

/**
 * Obtém o número de motoristas ativos
 */
int getNumMotoristasAtivos();

/**
 * Inicia um estado para um motorista
 * @param id ID do motorista
 * @param estado Novo estado a iniciar
 * @return true se iniciado com sucesso
 */
bool iniciarEstado(int id, EstadoJornada estado);

/**
 * Finaliza o estado atual de um motorista
 * @param id ID do motorista
 * @return true se finalizado com sucesso
 */
bool finalizarEstado(int id);

/**
 * Verifica se algum motorista tem jornada ativa
 * @return true se pelo menos um motorista está em jornada
 */
bool temJornadaAtiva();

/**
 * Verifica se algum motorista tem estado pausado ativo
 * (refeição, espera, descarga, abastecimento)
 * @return true se pelo menos um motorista está em estado pausado
 */
bool temEstadoPausadoAtivo();

/**
 * Obtém o nome do estado
 * @param estado Estado a converter
 * @return String com o nome do estado
 */
const char* getNomeEstado(EstadoJornada estado);

/**
 * Obtém estatísticas de um motorista
 * @param id ID do motorista
 * @param tempoAtual Tempo atual do estado (se ativo)
 * @return true se obtido com sucesso
 */
bool getEstatisticas(int id, unsigned long* tempoAtual);

/**
 * Callback para atualização da UI (deve ser implementado no arquivo principal)
 */
extern void onJornadaStateChange();

#endif // JORNADA_MANAGER_H

