/**
 * ============================================================================
 * GERENCIADOR DE ÁUDIO SIMPLIFICADO - IMPLEMENTAÇÃO ESP-IDF
 * ============================================================================
 *
 * NOTA: Esta é uma implementação stub. Para áudio real, integrar esp-adf
 * ou implementar decodificador MP3 com driver I2S nativo.
 *
 * ============================================================================
 */

#include "simple_audio_manager.h"
#include "pincfg.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "AUDIO";

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

bool isPlayingAudio = false;
SemaphoreHandle_t audioMutex = NULL;
static TaskHandle_t audioTaskHandle = NULL;

// Variáveis de controle interno
static volatile bool audioCommand = false;
static char pendingAudioFile[64] = "";
static int audioVolume = 15;

// Handle I2S
static i2s_chan_handle_t tx_handle = NULL;

// ============================================================================
// INICIALIZAÇÃO I2S
// ============================================================================

static esp_err_t init_i2s(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar canal I2S: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
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

    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar I2S STD: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao habilitar canal I2S: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2S inicializado com sucesso");
    return ESP_OK;
}

// ============================================================================
// TASK DE ÁUDIO
// ============================================================================

static void audioProcessTask(void* parameter) {
    // Inicializa I2S
    if (init_i2s() != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar I2S, task terminando");
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        // Verifica se há comando de áudio pendente
        if (xSemaphoreTake(audioMutex, 0) == pdTRUE) {
            if (audioCommand && strlen(pendingAudioFile) > 0) {
                // TODO: Implementar decodificação MP3 e reprodução
                // Por enquanto, apenas loga o arquivo solicitado
                ESP_LOGI(TAG, "Reproduzindo (stub): %s", pendingAudioFile);

                // Simula reprodução curta
                isPlayingAudio = true;
                xSemaphoreGive(audioMutex);

                // Simula tempo de reprodução
                vTaskDelay(pdMS_TO_TICKS(500));

                xSemaphoreTake(audioMutex, portMAX_DELAY);
                isPlayingAudio = false;
                audioCommand = false;
                pendingAudioFile[0] = '\0';
            }
            xSemaphoreGive(audioMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============================================================================
// FUNÇÕES PÚBLICAS
// ============================================================================

extern "C" void initSimpleAudio(void) {
    // Cria o mutex para sincronização
    if (audioMutex == NULL) {
        audioMutex = xSemaphoreCreateMutex();
        if (audioMutex == NULL) {
            ESP_LOGE(TAG, "Falha ao criar mutex de audio");
            return;
        }
    }

    // Cria a task de áudio no Core 1
    if (audioTaskHandle == NULL) {
        BaseType_t xReturned = xTaskCreatePinnedToCore(
            audioProcessTask,
            "AudioTask",
            8192,
            NULL,
            1,
            &audioTaskHandle,
            1  // Core 1
        );

        if (xReturned == pdPASS) {
            ESP_LOGI(TAG, "Task de audio criada no Core 1");
        } else {
            ESP_LOGE(TAG, "Falha ao criar task de audio");
        }
    }
}

extern "C" void playAudioFile(const char* filename) {
    if (filename == NULL || strlen(filename) == 0) {
        ESP_LOGW(TAG, "Nome de arquivo invalido");
        return;
    }

    // Monta o caminho completo
    char fullPath[128];
    snprintf(fullPath, sizeof(fullPath), "/littlefs%s", filename);

    // Verifica se o arquivo existe
    struct stat st;
    if (stat(fullPath, &st) != 0) {
        ESP_LOGW(TAG, "Arquivo nao encontrado: %s", fullPath);
        return;
    }

    // Envia comando para a task de áudio
    if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE) {
        strncpy(pendingAudioFile, filename, sizeof(pendingAudioFile) - 1);
        pendingAudioFile[sizeof(pendingAudioFile) - 1] = '\0';
        audioCommand = true;
        xSemaphoreGive(audioMutex);

        ESP_LOGI(TAG, "Comando de audio enviado: %s", filename);
    }
}

extern "C" void stopAudio(void) {
    if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE) {
        if (isPlayingAudio) {
            isPlayingAudio = false;
            ESP_LOGI(TAG, "Audio parado");
        }
        xSemaphoreGive(audioMutex);
    }
}

extern "C" bool isAudioPlaying(void) {
    bool playing = false;
    if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE) {
        playing = isPlayingAudio;
        xSemaphoreGive(audioMutex);
    }
    return playing;
}

extern "C" void setAudioVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 21) volume = 21;

    if (xSemaphoreTake(audioMutex, portMAX_DELAY) == pdTRUE) {
        audioVolume = volume;
        xSemaphoreGive(audioMutex);
        ESP_LOGI(TAG, "Volume definido: %d", audioVolume);
    }
}
