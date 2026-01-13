/**
 * ============================================================================
 * UTILITARIOS DE TEMPO - HEADER
 * ============================================================================
 *
 * Funcoes thread-safe para manipulacao e formatacao de tempo.
 *
 * ============================================================================
 */

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DEFINICOES
// ============================================================================

#define TIME_FORMAT_MIN_BUFFER  16  // Tamanho minimo do buffer para formatacao

// ============================================================================
// FUNCOES DE TEMPO
// ============================================================================

/**
 * Retorna o tempo em milissegundos desde o boot
 * @return Tempo em ms
 */
uint32_t time_millis(void);

/**
 * Retorna o tempo em microssegundos desde o boot
 * @return Tempo em us
 */
uint64_t time_micros(void);

/**
 * Retorna o tempo em segundos desde o boot
 * @return Tempo em segundos
 */
uint32_t time_seconds(void);

// ============================================================================
// FORMATACAO DE TEMPO (THREAD-SAFE)
// ============================================================================

/**
 * Formata tempo em ms para string HH:MM:SS ou MM:SS
 * @param timeMs Tempo em milissegundos
 * @param buffer Buffer para receber a string
 * @param bufferSize Tamanho do buffer (minimo TIME_FORMAT_MIN_BUFFER)
 */
void time_format_ms(uint32_t timeMs, char* buffer, size_t bufferSize);

/**
 * Formata tempo em ms para string HH:MM:SS.mmm
 * @param timeMs Tempo em milissegundos
 * @param buffer Buffer para receber a string
 * @param bufferSize Tamanho do buffer
 */
void time_format_ms_full(uint32_t timeMs, char* buffer, size_t bufferSize);

/**
 * Formata segundos para string HH:MM:SS ou MM:SS
 * @param totalSeconds Total de segundos
 * @param buffer Buffer para receber a string
 * @param bufferSize Tamanho do buffer
 */
void time_format_seconds(uint32_t totalSeconds, char* buffer, size_t bufferSize);

/**
 * Formata duracao entre dois timestamps
 * @param startMs Timestamp inicial
 * @param endMs Timestamp final
 * @param buffer Buffer para receber a string
 * @param bufferSize Tamanho do buffer
 */
void time_format_duration(uint32_t startMs, uint32_t endMs, char* buffer, size_t bufferSize);

// ============================================================================
// CONVERSOES
// ============================================================================

uint32_t time_hours_to_ms(uint32_t hours);
uint32_t time_minutes_to_ms(uint32_t minutes);
uint32_t time_seconds_to_ms(uint32_t seconds);

uint32_t time_ms_to_hours(uint32_t ms);
uint32_t time_ms_to_minutes(uint32_t ms);
uint32_t time_ms_to_seconds(uint32_t ms);

// ============================================================================
// COMPARACOES
// ============================================================================

/**
 * Verifica se um tempo passou desde o inicio
 * @param startMs Timestamp de inicio
 * @param durationMs Duracao a verificar
 * @return true se o tempo passou
 */
bool time_has_elapsed(uint32_t startMs, uint32_t durationMs);

/**
 * Retorna o tempo decorrido desde um timestamp
 * @param startMs Timestamp de inicio
 * @return Tempo decorrido em ms
 */
uint32_t time_elapsed_since(uint32_t startMs);

/**
 * Retorna o tempo restante
 * @param startMs Timestamp de inicio
 * @param durationMs Duracao total
 * @return Tempo restante em ms (0 se ja passou)
 */
uint32_t time_remaining(uint32_t startMs, uint32_t durationMs);

#ifdef __cplusplus
}

// ============================================================================
// CLASSE C++ PARA FORMATACAO (USA BUFFER INTERNO)
// ============================================================================

/**
 * Formatador de tempo com buffer interno
 * Cada instancia tem seu proprio buffer, evitando problemas de thread-safety
 */
class TimeFormatter {
public:
    TimeFormatter();

    const char* formatMs(uint32_t timeMs);
    const char* formatMsFull(uint32_t timeMs);
    const char* formatSeconds(uint32_t totalSeconds);
    const char* formatDuration(uint32_t startMs, uint32_t endMs);
    const char* formatElapsed(uint32_t startMs);

private:
    char buffer[32];
};

#endif // __cplusplus

#endif // TIME_UTILS_H
