/**
 * ============================================================================
 * GERENCIADOR DE ÁUDIO SIMPLIFICADO - IMPLEMENTAÇÃO COM MINIMP3
 * ============================================================================
 *
 * Versão robusta com:
 * - Alocação de memória segura (heap_caps)
 * - Verificações de NULL em todos os lugares
 * - Fila de áudio para não perder solicitações
 * - Interrupção imediata para novo áudio
 * - Task no Core 1 para não interferir com UI
 *
 * ============================================================================
 */

#include "simple_audio_manager.h"
#include "pincfg.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "driver/i2s_std.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3.h"

static const char *TAG = "AUDIO";

// ============================================================================
// CONFIGURAÇÕES
// ============================================================================

#define AUDIO_TASK_CORE         1           // Core dedicado para áudio
#define AUDIO_TASK_PRIORITY     5           // Prioridade alta
#define AUDIO_TASK_STACK_SIZE   (32 * 1024) // 32KB de stack

#define AUDIO_BUFFER_SIZE       (8 * 1024)  // 8KB para leitura MP3
#define PCM_BUFFER_SAMPLES      MINIMP3_MAX_SAMPLES_PER_FRAME  // Samples por frame

#define AUDIO_QUEUE_SIZE        4           // Tamanho da fila de áudio
#define I2S_WRITE_TIMEOUT_MS    100         // Timeout para I2S write

// ============================================================================
// ESTRUTURAS
// ============================================================================

typedef struct {
    char filename[64];
} AudioRequest_t;

typedef struct {
    // Handles e sincronização
    SemaphoreHandle_t mutex;
    QueueHandle_t queue;
    TaskHandle_t task_handle;
    i2s_chan_handle_t i2s_handle;

    // Buffers (alocados na heap)
    uint8_t* mp3_buffer;
    int16_t* pcm_buffer;
    mp3dec_t* decoder;

    // Estado
    volatile bool is_playing;
    volatile bool stop_requested;
    volatile int volume;  // 0-21
    int current_sample_rate;  // Taxa de amostragem atual

    // Flags de inicialização
    bool initialized;
    bool buffers_allocated;
    bool i2s_initialized;
} AudioManager_t;

// Instância única
static AudioManager_t* g_audio = NULL;

// Variáveis globais externas (compatibilidade)
bool isPlayingAudio = false;
SemaphoreHandle_t audioMutex = NULL;

// ============================================================================
// FUNÇÕES AUXILIARES DE MEMÓRIA
// ============================================================================

static void* audio_malloc(size_t size, uint32_t caps, const char* name) {
    void* ptr = heap_caps_malloc(size, caps);
    if (ptr) {
        ESP_LOGI(TAG, "Alocado %s: %d bytes em %p", name, (int)size, ptr);
    } else {
        ESP_LOGE(TAG, "FALHA ao alocar %s: %d bytes", name, (int)size);
    }
    return ptr;
}

static void audio_free(void** ptr) {
    if (ptr && *ptr) {
        heap_caps_free(*ptr);
        *ptr = NULL;
    }
}

// ============================================================================
// INICIALIZAÇÃO I2S
// ============================================================================

static esp_err_t audio_init_i2s(AudioManager_t* audio) {
    if (!audio) return ESP_ERR_INVALID_ARG;
    if (audio->i2s_initialized) return ESP_OK;

    ESP_LOGI(TAG, "Inicializando I2S...");

    // Configuração do canal
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;  // Limpa buffer automaticamente

    esp_err_t ret = i2s_new_channel(&chan_cfg, &audio->i2s_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar canal I2S: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configuração padrão I2S (MONO)
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)AUDIO_I2S_BCK_IO,
            .ws = (gpio_num_t)AUDIO_I2S_LRCK_IO,
            .dout = (gpio_num_t)AUDIO_I2S_DO_IO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(audio->i2s_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar modo STD: %s", esp_err_to_name(ret));
        i2s_del_channel(audio->i2s_handle);
        audio->i2s_handle = NULL;
        return ret;
    }

    ret = i2s_channel_enable(audio->i2s_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao habilitar I2S: %s", esp_err_to_name(ret));
        i2s_del_channel(audio->i2s_handle);
        audio->i2s_handle = NULL;
        return ret;
    }

    audio->i2s_initialized = true;
    audio->current_sample_rate = 44100;
    ESP_LOGI(TAG, "I2S inicializado com sucesso (44100Hz)");
    return ESP_OK;
}

// ============================================================================
// ALTERAR TAXA DE AMOSTRAGEM
// ============================================================================

static esp_err_t audio_set_sample_rate(AudioManager_t* audio, int sample_rate) {
    if (!audio || !audio->i2s_handle) return ESP_ERR_INVALID_ARG;
    if (audio->current_sample_rate == sample_rate) return ESP_OK;

    ESP_LOGI(TAG, "Alterando sample rate: %d -> %d Hz", audio->current_sample_rate, sample_rate);

    // Desabilita o canal
    i2s_channel_disable(audio->i2s_handle);

    // Reconfigura o clock
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)sample_rate);
    esp_err_t ret = i2s_channel_reconfig_std_clock(audio->i2s_handle, &clk_cfg);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao reconfigurar clock: %s", esp_err_to_name(ret));
        // Tenta reabilitar com config antiga
        i2s_channel_enable(audio->i2s_handle);
        return ret;
    }

    // Reabilita o canal
    ret = i2s_channel_enable(audio->i2s_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao reabilitar I2S: %s", esp_err_to_name(ret));
        return ret;
    }

    audio->current_sample_rate = sample_rate;
    return ESP_OK;
}

// ============================================================================
// ALOCAÇÃO DE BUFFERS
// ============================================================================

static esp_err_t audio_alloc_buffers(AudioManager_t* audio) {
    if (!audio) return ESP_ERR_INVALID_ARG;
    if (audio->buffers_allocated) return ESP_OK;

    ESP_LOGI(TAG, "Alocando buffers de audio...");

    // Buffer MP3 - memória interna
    audio->mp3_buffer = (uint8_t*)audio_malloc(
        AUDIO_BUFFER_SIZE,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
        "MP3 buffer"
    );
    if (!audio->mp3_buffer) goto fail;

    // Buffer PCM - DMA capable para I2S
    audio->pcm_buffer = (int16_t*)audio_malloc(
        PCM_BUFFER_SAMPLES * sizeof(int16_t) * 2,  // Stereo
        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA,
        "PCM buffer"
    );
    if (!audio->pcm_buffer) goto fail;

    // Decoder MP3 - memória interna (estrutura grande ~6KB)
    audio->decoder = (mp3dec_t*)audio_malloc(
        sizeof(mp3dec_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
        "MP3 decoder"
    );
    if (!audio->decoder) goto fail;

    // Limpa buffers
    memset(audio->mp3_buffer, 0, AUDIO_BUFFER_SIZE);
    memset(audio->pcm_buffer, 0, PCM_BUFFER_SAMPLES * sizeof(int16_t) * 2);
    memset(audio->decoder, 0, sizeof(mp3dec_t));

    audio->buffers_allocated = true;

    ESP_LOGI(TAG, "Buffers alocados com sucesso. Heap livre: %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    return ESP_OK;

fail:
    audio_free((void**)&audio->mp3_buffer);
    audio_free((void**)&audio->pcm_buffer);
    audio_free((void**)&audio->decoder);
    return ESP_ERR_NO_MEM;
}

// ============================================================================
// APLICAR VOLUME
// ============================================================================

static void audio_apply_volume(int16_t* pcm, int samples, int volume) {
    if (!pcm || samples <= 0) return;

    // Volume 0-21, onde 21 = 100%
    if (volume >= 21) return;  // Volume máximo

    if (volume <= 0) {
        memset(pcm, 0, samples * sizeof(int16_t));
        return;
    }

    // Escala quadrática para volume mais natural
    float scale = (float)volume / 21.0f;
    scale = scale * scale;

    for (int i = 0; i < samples; i++) {
        pcm[i] = (int16_t)(pcm[i] * scale);
    }
}

// ============================================================================
// REPRODUZIR ARQUIVO MP3
// ============================================================================

static void audio_play_file(AudioManager_t* audio, const char* filepath) {
    if (!audio || !filepath) return;

    // Verifica buffers
    if (!audio->mp3_buffer || !audio->pcm_buffer || !audio->decoder) {
        ESP_LOGE(TAG, "Buffers nao alocados!");
        return;
    }

    // Verifica I2S
    if (!audio->i2s_handle) {
        ESP_LOGE(TAG, "I2S nao inicializado!");
        return;
    }

    // Abre arquivo
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Falha ao abrir: %s", filepath);
        return;
    }

    // Tamanho do arquivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    ESP_LOGI(TAG, "Reproduzindo: %s (%ld bytes)", filepath, file_size);

    // Inicializa decoder
    mp3dec_init(audio->decoder);

    mp3dec_frame_info_t frame_info;
    size_t buffer_pos = 0;
    size_t buffer_len = 0;
    int current_volume;

    // Lê volume atual
    if (xSemaphoreTake(audio->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        current_volume = audio->volume;
        xSemaphoreGive(audio->mutex);
    } else {
        current_volume = 15;
    }

    // Lê dados iniciais
    buffer_len = fread(audio->mp3_buffer, 1, AUDIO_BUFFER_SIZE, file);

    // Detecta sample rate do primeiro frame
    bool first_frame = true;

    while (buffer_len > 0) {
        // Verifica stop
        if (audio->stop_requested) {
            ESP_LOGI(TAG, "Reproducao interrompida");
            break;
        }

        // Verifica se há dados suficientes
        size_t available = buffer_len - buffer_pos;
        if (available < 4) {
            break;  // Precisa de pelo menos 4 bytes para header MP3
        }

        // Limpa frame_info
        memset(&frame_info, 0, sizeof(frame_info));

        // Decodifica frame
        int samples = mp3dec_decode_frame(
            audio->decoder,
            audio->mp3_buffer + buffer_pos,
            available,
            audio->pcm_buffer,
            &frame_info
        );

        if (frame_info.frame_bytes > 0) {
            buffer_pos += frame_info.frame_bytes;

            if (samples > 0 && frame_info.channels > 0 && frame_info.channels <= 2) {
                // Log do primeiro frame
                if (first_frame) {
                    ESP_LOGI(TAG, "MP3 Info: %dHz, %d canais, layer %d, %d kbps",
                             frame_info.hz, frame_info.channels, frame_info.layer, frame_info.bitrate_kbps);
                    first_frame = false;
                }

                // Ajusta sample rate se necessário
                if (frame_info.hz > 0 && frame_info.hz != audio->current_sample_rate) {
                    audio_set_sample_rate(audio, frame_info.hz);
                }

                // Atualiza volume se mudou
                if (audio->mutex && xSemaphoreTake(audio->mutex, 0) == pdTRUE) {
                    current_volume = audio->volume;
                    xSemaphoreGive(audio->mutex);
                }

                // Aplica volume (usa apenas samples mono)
                audio_apply_volume(audio->pcm_buffer, samples, current_volume);

                // Verifica limites
                if (samples > 0 && samples <= PCM_BUFFER_SAMPLES) {
                    // Envia para I2S (MONO)
                    size_t bytes_written = 0;
                    esp_err_t ret = i2s_channel_write(
                        audio->i2s_handle,
                        audio->pcm_buffer,
                        samples * sizeof(int16_t),
                        &bytes_written,
                        pdMS_TO_TICKS(I2S_WRITE_TIMEOUT_MS)
                    );

                    if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
                        ESP_LOGW(TAG, "Erro I2S write: %s", esp_err_to_name(ret));
                    }
                }
            }
        } else if (frame_info.frame_bytes == 0) {
            // Não encontrou frame válido, avança 1 byte
            buffer_pos++;
            if (buffer_pos >= buffer_len) {
                break;
            }
        }

        // Recarrega buffer se necessário
        if (buffer_pos > AUDIO_BUFFER_SIZE / 2) {
            size_t remaining = buffer_len - buffer_pos;
            if (remaining > 0) {
                memmove(audio->mp3_buffer, audio->mp3_buffer + buffer_pos, remaining);
            }

            size_t new_bytes = fread(
                audio->mp3_buffer + remaining,
                1,
                AUDIO_BUFFER_SIZE - remaining,
                file
            );

            buffer_len = remaining + new_bytes;
            buffer_pos = 0;

            if (new_bytes == 0 && remaining == 0) {
                break;  // Fim do arquivo
            }
        }

        // Yield
        taskYIELD();
    }

    fclose(file);
    ESP_LOGI(TAG, "Reproducao finalizada: %s", filepath);
}

// ============================================================================
// TASK DE ÁUDIO (CORE 1)
// ============================================================================

static void audio_task(void* arg) {
    AudioManager_t* audio = (AudioManager_t*)arg;

    if (!audio) {
        ESP_LOGE(TAG, "Audio task: argumento invalido!");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Audio task iniciada no Core %d", xPortGetCoreID());

    // Aloca buffers
    if (audio_alloc_buffers(audio) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao alocar buffers!");
        vTaskDelete(NULL);
        return;
    }

    // Inicializa I2S
    if (audio_init_i2s(audio) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar I2S!");
        vTaskDelete(NULL);
        return;
    }

    AudioRequest_t request;
    char filepath[128];

    while (true) {
        // Espera por solicitação
        if (xQueueReceive(audio->queue, &request, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Pega o mais recente se houver mais na fila
            AudioRequest_t newest = request;
            while (xQueueReceive(audio->queue, &request, 0) == pdTRUE) {
                newest = request;
            }

            // Monta caminho
            snprintf(filepath, sizeof(filepath), "/littlefs%s", newest.filename);

            // Marca como reproduzindo
            if (xSemaphoreTake(audio->mutex, portMAX_DELAY) == pdTRUE) {
                audio->stop_requested = false;
                audio->is_playing = true;
                isPlayingAudio = true;
                xSemaphoreGive(audio->mutex);
            }

            // Reproduz
            audio_play_file(audio, filepath);

            // Marca como não reproduzindo
            if (xSemaphoreTake(audio->mutex, portMAX_DELAY) == pdTRUE) {
                audio->is_playing = false;
                isPlayingAudio = false;
                xSemaphoreGive(audio->mutex);
            }
        }
    }
}

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

extern "C" void initSimpleAudio(void) {
    if (g_audio && g_audio->initialized) {
        ESP_LOGW(TAG, "Audio ja inicializado!");
        return;
    }

    ESP_LOGI(TAG, "Inicializando sistema de audio...");

    // Aloca estrutura principal
    g_audio = (AudioManager_t*)audio_malloc(
        sizeof(AudioManager_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
        "AudioManager"
    );

    if (!g_audio) {
        ESP_LOGE(TAG, "Falha ao alocar AudioManager!");
        return;
    }

    memset(g_audio, 0, sizeof(AudioManager_t));
    g_audio->volume = 21;  // Volume máximo

    // Cria mutex
    g_audio->mutex = xSemaphoreCreateMutex();
    if (!g_audio->mutex) {
        ESP_LOGE(TAG, "Falha ao criar mutex!");
        audio_free((void**)&g_audio);
        return;
    }
    audioMutex = g_audio->mutex;  // Compatibilidade

    // Cria fila
    g_audio->queue = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(AudioRequest_t));
    if (!g_audio->queue) {
        ESP_LOGE(TAG, "Falha ao criar fila!");
        vSemaphoreDelete(g_audio->mutex);
        audio_free((void**)&g_audio);
        return;
    }

    // Cria task no Core 1
    BaseType_t ret = xTaskCreatePinnedToCore(
        audio_task,
        "AudioTask",
        AUDIO_TASK_STACK_SIZE,
        g_audio,
        AUDIO_TASK_PRIORITY,
        &g_audio->task_handle,
        AUDIO_TASK_CORE
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar task de audio!");
        vQueueDelete(g_audio->queue);
        vSemaphoreDelete(g_audio->mutex);
        audio_free((void**)&g_audio);
        return;
    }

    g_audio->initialized = true;
    ESP_LOGI(TAG, "Sistema de audio inicializado (Core %d)", AUDIO_TASK_CORE);
}

extern "C" void playAudioFile(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        ESP_LOGW(TAG, "Nome de arquivo invalido");
        return;
    }

    if (!g_audio) {
        ESP_LOGE(TAG, "Audio nao inicializado!");
        return;
    }

    if (!g_audio->initialized || !g_audio->queue) {
        ESP_LOGE(TAG, "Audio nao pronto!");
        return;
    }

    // Verifica se arquivo existe
    char fullpath[128];
    snprintf(fullpath, sizeof(fullpath), "/littlefs%s", filename);

    struct stat st;
    if (stat(fullpath, &st) != 0) {
        ESP_LOGW(TAG, "Arquivo nao encontrado: %s", fullpath);
        return;
    }

    // Sinaliza stop para áudio atual (sem mutex para evitar deadlock)
    g_audio->stop_requested = true;

    // Envia para fila
    AudioRequest_t request;
    strncpy(request.filename, filename, sizeof(request.filename) - 1);
    request.filename[sizeof(request.filename) - 1] = '\0';

    if (xQueueSend(g_audio->queue, &request, 0) != pdTRUE) {
        // Fila cheia - remove o mais antigo
        AudioRequest_t dummy;
        xQueueReceive(g_audio->queue, &dummy, 0);
        xQueueSend(g_audio->queue, &request, 0);
    }

    ESP_LOGI(TAG, "Audio solicitado: %s", filename);
}

extern "C" void stopAudio(void) {
    if (!g_audio || !g_audio->initialized) return;

    if (xSemaphoreTake(g_audio->mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (g_audio->is_playing) {
            g_audio->stop_requested = true;
            ESP_LOGI(TAG, "Stop solicitado");
        }
        xSemaphoreGive(g_audio->mutex);
    }
}

extern "C" bool isAudioPlaying(void) {
    if (!g_audio || !g_audio->initialized) return false;

    bool playing = false;
    if (xSemaphoreTake(g_audio->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        playing = g_audio->is_playing;
        xSemaphoreGive(g_audio->mutex);
    }
    return playing;
}

extern "C" void setAudioVolume(int volume) {
    if (!g_audio || !g_audio->initialized) return;

    if (volume < 0) volume = 0;
    if (volume > 21) volume = 21;

    if (xSemaphoreTake(g_audio->mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        g_audio->volume = volume;
        xSemaphoreGive(g_audio->mutex);
        ESP_LOGI(TAG, "Volume: %d", volume);
    }
}
