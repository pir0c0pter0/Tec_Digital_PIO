/**
 * ============================================================================
 * GERENCIADOR DE ÁUDIO SIMPLIFICADO (ESP-IDF)
 * ============================================================================
 */

#ifndef SIMPLE_AUDIO_MANAGER_H
#define SIMPLE_AUDIO_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// VARIÁVEIS EXTERNAS
// ============================================================================

extern bool isPlayingAudio;
extern SemaphoreHandle_t audioMutex;

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

/**
 * Inicializa o sistema de áudio simplificado
 */
void initSimpleAudio(void);

/**
 * Reproduz um arquivo de áudio da flash (LittleFS)
 * @param filename Nome do arquivo (ex: "/ign_on.mp3")
 */
void playAudioFile(const char* filename);

/**
 * Para a reprodução de áudio atual
 */
void stopAudio(void);

/**
 * Verifica se há áudio tocando
 * @return true se está tocando, false caso contrário
 */
bool isAudioPlaying(void);

/**
 * Define o volume do áudio
 * @param volume Volume de 0 a 21
 */
void setAudioVolume(int volume);

#ifdef __cplusplus
}
#endif

#endif // SIMPLE_AUDIO_MANAGER_H
