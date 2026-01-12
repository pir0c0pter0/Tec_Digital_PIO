/**
 * ============================================================================
 * SISTEMA DE CONTROLE - ESP32-S3 (ESP-IDF PURO)
 * COM DUAL KEYBOARD E SWIPE NAVIGATION
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_littlefs.h"
#include <dirent.h>
#include "lvgl.h"

#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "ignicao_control.h"
#include "simple_audio_manager.h"
#include "simple_splash.h"
#include "button_manager.h"
#include "numpad_example.h"
#include "jornada_keyboard.h"
#include "lvgl_fs_driver.h"

// ============================================================================
// DEFINIÇÕES
// ============================================================================

static const char *TAG = "MAIN";

#define LVGL_PORT_ROTATION_DEGREE (90)

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

static bool systemInitialized = false;
static bool ignicaoLigada = false;
static uint32_t ignicaoStartTime = 0;

// Referências externas (definidas em C++)
extern "C" {
extern void* btnManager;
extern void* screenManager;

// Funções externas
void buttonManagerInit(void);
void* buttonManagerGetInstance(void);
void buttonManagerUpdateStatusBar(bool ignicaoOn, uint32_t tempoIgnicao, uint32_t tempoJornada, const char* msg);
void numpadInit(void);
void initScreenManager(void);
} // extern "C"

// ============================================================================
// FUNÇÕES AUXILIARES
// ============================================================================

static inline uint32_t millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// ============================================================================
// CALLBACK DE MUDANÇA DE IGNIÇÃO
// ============================================================================

void onIgnicaoStatusChange(bool newStatus) {
    if (!systemInitialized) return;

    if (newStatus) {
        ESP_LOGI(TAG, "==================");
        ESP_LOGI(TAG, "IGNICAO LIGADA");
        ESP_LOGI(TAG, "==================");

        playAudioFile("/ign_on_jornada_manobra.mp3");

        if (!ignicaoLigada) {
            ignicaoStartTime = millis();
            ignicaoLigada = true;
        }

    } else {
        ESP_LOGI(TAG, "==================");
        ESP_LOGI(TAG, "IGNICAO DESLIGADA");
        ESP_LOGI(TAG, "==================");

        playAudioFile("/ign_off.mp3");

        ignicaoLigada = false;
    }

    // Atualiza barra de status
    if (btnManager) {
        uint32_t tempoIgnicao = ignicaoLigada ? (millis() - ignicaoStartTime) : 0;
        buttonManagerUpdateStatusBar(newStatus, tempoIgnicao, 0, NULL);
    }
}

// ============================================================================
// INICIALIZAÇÃO DO LITTLEFS
// ============================================================================

static esp_err_t init_littlefs(void) {
    ESP_LOGI(TAG, "Inicializando LittleFS...");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "spiffs",  // Nome da partição (pode ser littlefs ou spiffs)
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Falha ao montar ou formatar filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Partição não encontrada");
        } else {
            ESP_LOGE(TAG, "Falha ao inicializar LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS montado! Total: %d bytes, Usado: %d bytes", total, used);
    }

    return ESP_OK;
}

// ============================================================================
// LISTAR ARQUIVOS
// ============================================================================

static void list_files(void) {
    ESP_LOGI(TAG, "Listando arquivos no LittleFS...");

    DIR *dir = opendir("/littlefs");
    if (!dir) {
        ESP_LOGE(TAG, "Erro ao abrir diretório raiz");
        return;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            ESP_LOGI(TAG, "  Arquivo: %s", entry->d_name);
            count++;
        }
    }

    closedir(dir);

    if (count == 0) {
        ESP_LOGW(TAG, "Nenhum arquivo encontrado");
    } else {
        ESP_LOGI(TAG, "Total de arquivos: %d", count);
    }
}

// ============================================================================
// TASK PRINCIPAL DO SISTEMA
// ============================================================================

static void system_task(void *arg) {
    // Aguarda splash terminar
    while (!isSplashDone()) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Completando inicializacao...");

    // Inicialização do sistema de áudio
    initSimpleAudio();

    // Criação da interface
    buttonManagerInit();
    btnManager = buttonManagerGetInstance();

    // Inicializar NumpadExample
    numpadInit();

    // Inicializar o gerenciador de telas
    initScreenManager();

    // Inicialização do controle de ignição
    if (initIgnicaoControl(1.0, 2.0, true)) {
        bool initialState = getIgnicaoStatus();
        ESP_LOGI(TAG, "Estado inicial da ignicao: %s", initialState ? "ON" : "OFF");

        if (initialState) {
            ignicaoStartTime = millis();
            ignicaoLigada = true;
        }
    }

    systemInitialized = true;

    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "Sistema Pronto!");
    ESP_LOGI(TAG, "- Teclado Numerico: Ativo");
    ESP_LOGI(TAG, "- Swipe esquerda: Teclado Jornada");
    ESP_LOGI(TAG, "- Swipe direita: Teclado Numerico");
    ESP_LOGI(TAG, "=================================");

    // Loop principal
    uint32_t lastUpdate = 0;

    while (1) {
        uint32_t now = millis();

        // Atualização periódica (a cada 1 segundo)
        if ((now - lastUpdate) >= 1000) {
            lastUpdate = now;

            if (btnManager) {
                bool ignicaoOn = getIgnicaoStatus();
                uint32_t tempoIgnicao = 0;

                if (ignicaoLigada && ignicaoOn) {
                    tempoIgnicao = now - ignicaoStartTime;
                }

                buttonManagerUpdateStatusBar(ignicaoOn, tempoIgnicao, 0, NULL);
            }
        }

        // Processa eventos LVGL
        lv_timer_handler();

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ============================================================================
// APP_MAIN - PONTO DE ENTRADA ESP-IDF
// ============================================================================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "Sistema ESP32-S3 ESP-IDF");
    ESP_LOGI(TAG, "Versao 2.0 - Dual Keyboard");
    ESP_LOGI(TAG, "=================================");

    // Inicializa LittleFS
    if (init_littlefs() != ESP_OK) {
        ESP_LOGE(TAG, "ERRO: Falha ao montar LittleFS!");
        return;
    }

    // Lista arquivos disponíveis
    list_files();

    // Inicializa Display
    ESP_LOGI(TAG, "Inicializando display...");

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
        .rotate = LV_DISP_ROT_90,
    };

    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    // Inicializa driver de filesystem LVGL
    lvgl_fs_init('A');

    ESP_LOGI(TAG, "Display inicializado!");

    // Exibe splash screen
    ESP_LOGI(TAG, "Exibindo splash screen...");
    createSplashScreen();

    // Cria task principal do sistema
    xTaskCreatePinnedToCore(
        system_task,
        "system_task",
        8192,
        NULL,
        5,
        NULL,
        0  // Core 0
    );
}
