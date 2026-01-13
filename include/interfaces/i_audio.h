/**
 * ============================================================================
 * INTERFACE DE AUDIO
 * ============================================================================
 *
 * Abstrai o sistema de reproducao de audio.
 * Permite trocar a implementacao sem afetar o resto do sistema.
 *
 * ============================================================================
 */

#ifndef I_AUDIO_H
#define I_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus

/**
 * Interface abstrata para player de audio
 */
class IAudioPlayer {
public:
    virtual ~IAudioPlayer() = default;

    /**
     * Inicializa o sistema de audio
     * @return true se inicializado com sucesso
     */
    virtual bool init() = 0;

    /**
     * Reproduz um arquivo de audio
     * @param filename Caminho do arquivo (ex: "/click.mp3")
     */
    virtual void play(const char* filename) = 0;

    /**
     * Para a reproducao atual
     */
    virtual void stop() = 0;

    /**
     * Verifica se esta reproduzindo
     * @return true se esta reproduzindo audio
     */
    virtual bool isPlaying() const = 0;

    /**
     * Define o volume
     * @param volume Valor de 0 (mudo) a 21 (maximo)
     */
    virtual void setVolume(int volume) = 0;

    /**
     * Obtem o volume atual
     * @return Volume atual (0-21)
     */
    virtual int getVolume() const = 0;

    /**
     * Verifica se o sistema esta inicializado
     * @return true se inicializado
     */
    virtual bool isInitialized() const = 0;
};

extern "C" {
#endif

// Interface C para compatibilidade
void audio_init(void);
void audio_play(const char* filename);
void audio_stop(void);
bool audio_is_playing(void);
void audio_set_volume(int volume);
int audio_get_volume(void);

#ifdef __cplusplus
}
#endif

#endif // I_AUDIO_H
