/**
 * ============================================================================
 * INTERFACE DE IGNICAO
 * ============================================================================
 *
 * Abstrai o monitoramento da ignicao do veiculo.
 *
 * ============================================================================
 */

#ifndef I_IGNICAO_H
#define I_IGNICAO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus

/**
 * Callback para mudanca de estado da ignicao
 */
typedef void (*IgnicaoCallback)(bool newStatus);

/**
 * Estatisticas de ignicao
 */
struct IgnicaoStats {
    uint32_t totalOnTime;       // Tempo total ligada (ms)
    uint32_t totalOffTime;      // Tempo total desligada (ms)
    uint32_t lastChangeTime;    // Timestamp da ultima mudanca
    uint32_t sessionStartTime;  // Inicio da sessao atual
};

/**
 * Interface abstrata para controle de ignicao
 */
class IIgnicaoService {
public:
    virtual ~IIgnicaoService() = default;

    /**
     * Inicializa o servico de ignicao
     * @param debounceOn Tempo de debounce para ON (segundos)
     * @param debounceOff Tempo de debounce para OFF (segundos)
     * @return true se inicializado com sucesso
     */
    virtual bool init(float debounceOn, float debounceOff) = 0;

    /**
     * Obtem o status atual da ignicao
     * @return true se ligada
     */
    virtual bool getStatus() const = 0;

    /**
     * Obtem o estado bruto do pino (sem debounce)
     * @return true se pino esta em nivel alto
     */
    virtual bool getRawStatus() const = 0;

    /**
     * Define o tempo de debounce
     * @param debounceOn Tempo para ON (segundos)
     * @param debounceOff Tempo para OFF (segundos)
     */
    virtual void setDebounce(float debounceOn, float debounceOff) = 0;

    /**
     * Obtem os tempos de debounce atuais
     * @param debounceOn Ponteiro para receber tempo ON
     * @param debounceOff Ponteiro para receber tempo OFF
     */
    virtual void getDebounce(float* debounceOn, float* debounceOff) const = 0;

    /**
     * Registra callback para mudanca de estado
     * @param callback Funcao a ser chamada
     */
    virtual void setCallback(IgnicaoCallback callback) = 0;

    /**
     * Obtem estatisticas de uso
     * @return Estrutura com estatisticas
     */
    virtual IgnicaoStats getStats() const = 0;

    /**
     * Reseta as estatisticas
     */
    virtual void resetStats() = 0;

    /**
     * Inicia o monitoramento
     */
    virtual void start() = 0;

    /**
     * Para o monitoramento
     */
    virtual void stop() = 0;

    /**
     * Verifica se esta monitorando
     * @return true se task esta rodando
     */
    virtual bool isRunning() const = 0;
};

extern "C" {
#endif

// Interface C para compatibilidade
bool ignicao_init(float debounceOn, float debounceOff);
bool ignicao_get_status(void);
int ignicao_get_raw_status(void);
void ignicao_set_debounce(float debounceOn, float debounceOff);
void ignicao_start(void);
void ignicao_stop(void);

#ifdef __cplusplus
}
#endif

#endif // I_IGNICAO_H
