/**
 * ============================================================================
 * INTERFACE DE JORNADA
 * ============================================================================
 *
 * Abstrai o gerenciamento de jornada dos motoristas.
 *
 * ============================================================================
 */

#ifndef I_JORNADA_H
#define I_JORNADA_H

#include <stdbool.h>
#include <stdint.h>
#define MAX_NOME_MOTORISTA 32  // Define local para evitar dependencia circular

#ifdef __cplusplus

/**
 * Estados possiveis de jornada
 */
enum class EstadoJornada : uint8_t {
    INATIVO = 0,
    JORNADA,        // Direcao
    MANOBRA,
    REFEICAO,
    ESPERA,
    DESCARGA,
    ABASTECIMENTO,
    MAX_ESTADOS
};

/**
 * Dados de um motorista
 */
struct DadosMotorista {
    int id;
    char nome[MAX_NOME_MOTORISTA];
    EstadoJornada estadoAtual;
    uint32_t tempoInicio;
    uint32_t tempoTotalJornada;
    uint32_t tempoTotalManobra;
    uint32_t tempoTotalRefeicao;
    uint32_t tempoTotalEspera;
    uint32_t tempoTotalDescarga;
    uint32_t tempoTotalAbastecimento;
    bool ativo;
};

/**
 * Callback para mudanca de estado
 */
typedef void (*JornadaCallback)(int motoristaId, EstadoJornada novoEstado);

/**
 * Interface abstrata para servico de jornada
 */
class IJornadaService {
public:
    virtual ~IJornadaService() = default;

    /**
     * Inicializa o servico
     */
    virtual void init() = 0;

    /**
     * Adiciona um motorista
     * @param id ID do motorista (1-3)
     * @param nome Nome do motorista
     * @return true se adicionado com sucesso
     */
    virtual bool addMotorista(int id, const char* nome) = 0;

    /**
     * Remove um motorista
     * @param id ID do motorista
     */
    virtual void removeMotorista(int id) = 0;

    /**
     * Obtem dados de um motorista (copia thread-safe)
     * @param id ID do motorista
     * @param dados Ponteiro para receber os dados
     * @return true se encontrado
     */
    virtual bool getMotorista(int id, DadosMotorista* dados) const = 0;

    /**
     * Obtem numero de motoristas ativos
     * @return Quantidade de motoristas
     */
    virtual int getNumMotoristasAtivos() const = 0;

    /**
     * Inicia um estado para um motorista
     * @param id ID do motorista
     * @param estado Novo estado
     * @return true se iniciado com sucesso
     */
    virtual bool iniciarEstado(int id, EstadoJornada estado) = 0;

    /**
     * Finaliza o estado atual de um motorista
     * @param id ID do motorista
     * @return true se finalizado
     */
    virtual bool finalizarEstado(int id) = 0;

    /**
     * Verifica se algum motorista tem jornada ativa
     * @return true se pelo menos um em JORNADA
     */
    virtual bool temJornadaAtiva() const = 0;

    /**
     * Verifica se algum motorista esta em estado pausado
     * @return true se algum em REFEICAO, ESPERA, etc.
     */
    virtual bool temEstadoPausadoAtivo() const = 0;

    /**
     * Obtem o nome de um estado
     * @param estado Estado a converter
     * @return String com nome
     */
    virtual const char* getNomeEstado(EstadoJornada estado) const = 0;

    /**
     * Obtem tempo atual do estado de um motorista
     * @param id ID do motorista
     * @return Tempo em ms (0 se inativo)
     */
    virtual uint32_t getTempoEstadoAtual(int id) const = 0;

    /**
     * Registra callback para mudanca de estado
     * @param callback Funcao a ser chamada
     */
    virtual void setCallback(JornadaCallback callback) = 0;
};

extern "C" {
#endif

// Enum C compativel
typedef enum {
    ESTADO_JORNADA_INATIVO = 0,
    ESTADO_JORNADA_DIRECAO,
    ESTADO_JORNADA_MANOBRA,
    ESTADO_JORNADA_REFEICAO,
    ESTADO_JORNADA_ESPERA,
    ESTADO_JORNADA_DESCARGA,
    ESTADO_JORNADA_ABASTECIMENTO
} estado_jornada_t;

// Interface C para compatibilidade
void jornada_init(void);
bool jornada_add_motorista(int id, const char* nome);
void jornada_remove_motorista(int id);
bool jornada_iniciar_estado(int id, estado_jornada_t estado);
bool jornada_finalizar_estado(int id);
bool jornada_tem_ativa(void);
const char* jornada_get_nome_estado(estado_jornada_t estado);

#ifdef __cplusplus
}
#endif

#endif // I_JORNADA_H
