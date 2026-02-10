/**
 * ============================================================================
 * TECLADO DE JORNADA DIGITAL - PONTO DE ENTRADA
 * ============================================================================
 *
 * Sistema embarcado para monitoramento de jornada de motoristas.
 * Utiliza ScreenManagerImpl para navegacao entre telas com animacao.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-IDF
#include "esp_log.h"
#include "esp_timer.h"

// Configuracao centralizada
#include "config/app_config.h"

// Core
#include "core/app_init.h"

// Utils
#include "utils/time_utils.h"
#include "utils/debug_utils.h"

// Display e UI
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "lvgl.h"

// Audio e Splash
#include "simple_audio_manager.h"
#include "simple_splash.h"

// Ignicao e Filesystem
#include "ignicao_control.h"
#include "lvgl_fs_driver.h"

// Persistencia NVS
#include "services/nvs/nvs_manager.h"

// BLE
#include "services/ble/ble_service.h"
#include "services/ble/ble_event_queue.h"
#include "services/ble/gatt/gatt_journey.h"
#include "services/ble/gatt/gatt_config.h"

// Nova arquitetura de telas
#include "ui/screen_manager.h"
#include "ui/widgets/status_bar.h"
#include "ui/screens/jornada_screen.h"
#include "ui/screens/numpad_screen.h"
#include "ui/screens/settings_screen.h"

#include <sys/time.h>

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char *TAG = "MAIN";

// ============================================================================
// VARIAVEIS GLOBAIS
// ============================================================================

static bool systemInitialized = false;
static bool ignicaoLigada = false;
static uint32_t ignicaoStartTime = 0;

// StatusBar persistente (alocacao estatica)
static StatusBar statusBar;

// Telas (alocacao estatica)
static JornadaScreen jornadaScreen;
static NumpadScreen numpadScreen;
static SettingsScreen settingsScreen;

// Ponteiro para screen manager (para uso no loop)
static ScreenManagerImpl* screenMgr = nullptr;

// ============================================================================
// CALLBACK DE IGNICAO
// ============================================================================

void onIgnicaoStatusChange(bool newStatus) {
    if (!systemInitialized) return;

    if (newStatus) {
        ESP_LOGI(TAG, "==================");
        ESP_LOGI(TAG, "IGNICAO LIGADA");
        ESP_LOGI(TAG, "==================");

        playAudioFile(AUDIO_FILE_IGN_ON);

        if (!ignicaoLigada) {
            ignicaoStartTime = time_millis();
            ignicaoLigada = true;
        }
    } else {
        ESP_LOGI(TAG, "==================");
        ESP_LOGI(TAG, "IGNICAO DESLIGADA");
        ESP_LOGI(TAG, "==================");

        playAudioFile(AUDIO_FILE_IGN_OFF);
        ignicaoLigada = false;
    }

    // Atualiza barra de status via StatusBar diretamente
    uint32_t tempoIgnicao = ignicaoLigada ? (time_millis() - ignicaoStartTime) : 0;
    statusBar.setIgnicao(newStatus, tempoIgnicao);

    // Notifica cliente BLE sobre mudanca de ignicao
    notify_ignition_state();
}

// Callback de jornada (requerido pelo jornada_manager)
void onJornadaStateChange(void) {
    ESP_LOGD(TAG, "Estado de jornada alterado");

    // Notifica cliente BLE sobre mudanca de jornada
    notify_journey_state();
}

// ============================================================================
// HANDLER DE EVENTOS BLE (chamado do system_task -- LVGL-safe)
// ============================================================================

static void onBleEvent(const BleEvent& evt) {
    // Atualiza icone BLE na StatusBar (LVGL-safe -- estamos no system_task Core 0)
    statusBar.setBleStatus(evt.status);

    switch (evt.status) {
        case BleStatus::DISCONNECTED:
            ESP_LOGI(TAG, "BLE: Desconectado");
            break;
        case BleStatus::ADVERTISING:
            ESP_LOGI(TAG, "BLE: Advertising...");
            break;
        case BleStatus::CONNECTED:
            ESP_LOGI(TAG, "BLE: Conectado (handle=%d)", evt.conn_handle);
            break;
        case BleStatus::SECURED:
            ESP_LOGI(TAG, "BLE: Conexao segura (handle=%d)", evt.conn_handle);
            break;
    }
}

// ============================================================================
// HANDLER DE EVENTOS DE CONFIGURACAO BLE (chamado do system_task -- LVGL-safe)
// ============================================================================

static void onConfigEvent(const ConfigEvent& evt) {
    auto* nvsMgr = NvsManager::getInstance();

    switch (evt.type) {
        case CONFIG_EVT_VOLUME: {
            ESP_LOGI(TAG, "Config BLE: volume=%d", evt.value_u8);
            // 1. Aplica no hardware
            setAudioVolume(evt.value_u8);
            // 2. Persiste no NVS
            nvsMgr->saveVolume(evt.value_u8);
            // 3. Notifica cliente BLE (echo back)
            notify_config_volume();
            // 4. Atualiza SettingsScreen UI se ativa
            if (screenMgr && screenMgr->getCurrentScreen() == ScreenType::SETTINGS) {
                settingsScreen.updateVolumeSlider(evt.value_u8);
            }
            break;
        }
        case CONFIG_EVT_BRIGHTNESS: {
            ESP_LOGI(TAG, "Config BLE: brightness=%d", evt.value_u8);
            bsp_display_brightness_set(evt.value_u8);
            nvsMgr->saveBrightness(evt.value_u8);
            notify_config_brightness();
            if (screenMgr && screenMgr->getCurrentScreen() == ScreenType::SETTINGS) {
                settingsScreen.updateBrightnessSlider(evt.value_u8);
            }
            break;
        }
        case CONFIG_EVT_DRIVER_NAME: {
            ESP_LOGI(TAG, "Config BLE: driver %d name='%s'", evt.driver_id, evt.name);
            nvsMgr->saveDriverName(evt.driver_id, evt.name);
            break;
        }
        case CONFIG_EVT_TIME_SYNC: {
            ESP_LOGI(TAG, "Config BLE: time sync=%lu", (unsigned long)evt.value_u32);
            struct timeval tv = {
                .tv_sec = (time_t)evt.value_u32,
                .tv_usec = 0,
            };
            settimeofday(&tv, NULL);
            break;
        }
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

    // Inicializa NVS (deve ser antes de qualquer leitura de configuracao)
    auto* nvsMgr = NvsManager::getInstance();
    if (!nvsMgr->init()) {
        ESP_LOGE(TAG, "Falha ao inicializar NVS! Usando valores padrao.");
    }

    // Inicializa subsistema de audio
    initSimpleAudio();

    // Restaura volume salvo no NVS
    uint8_t savedVolume = nvsMgr->loadVolume(AUDIO_VOLUME_DEFAULT);
    ESP_LOGI(TAG, "Volume restaurado: %d", savedVolume);
    setAudioVolume(savedVolume);

    // Restaura brilho salvo no NVS
    uint8_t savedBrightness = nvsMgr->loadBrightness(100);
    ESP_LOGI(TAG, "Brilho restaurado: %d%%", savedBrightness);
    bsp_display_brightness_set(savedBrightness);

    // Controle de ignicao (antes da UI, para saber estado inicial)
    if (initIgnicaoControl(IGNICAO_DEBOUNCE_ON_S, IGNICAO_DEBOUNCE_OFF_S, true)) {
        bool initialState = getIgnicaoStatus();
        ESP_LOGI(TAG, "Estado inicial da ignicao: %s", initialState ? "ON" : "OFF");

        if (initialState) {
            ignicaoStartTime = time_millis();
            ignicaoLigada = true;
        }
    }

    // Cria e inicializa StatusBar no lv_layer_top()
    statusBar.create();

    // Cria e inicializa ScreenManager
    screenMgr = ScreenManagerImpl::getInstance();
    screenMgr->init();
    screenMgr->setStatusBar(&statusBar);

    // Conecta StatusBar ao ScreenManager (para callbacks menu/back)
    statusBar.setScreenManager(screenMgr);

    // Registra telas
    screenMgr->registerScreen(&jornadaScreen);
    screenMgr->registerScreen(&numpadScreen);

    // Registra SettingsScreen
    screenMgr->registerScreen(&settingsScreen);

    // Pre-cria todas as telas no boot para troca instantanea
    // (evita lag na primeira navegacao)
    jornadaScreen.create();
    numpadScreen.create();
    settingsScreen.create();

    // Conecta StatusBar as telas via metodos per-screen (sem singleton)
    numpadScreen.setStatusBar(&statusBar);
    jornadaScreen.setStatusBar(&statusBar);

    // Mostra tela inicial (Numpad, sem animacao)
    screenMgr->showInitialScreen(ScreenType::NUMPAD);

    // Deleta o objeto LVGL do splash (agora que temos uma tela ativa)
    deleteSplashScreen();

    systemInitialized = true;

    // Inicializa BLE (apos NVS e UI para nao bloquear boot)
    auto* bleSvc = BleService::getInstance();
    if (!bleSvc->init()) {
        ESP_LOGE(TAG, "Falha ao inicializar BLE!");
    } else {
        ESP_LOGI(TAG, "BLE inicializado - advertising...");
        statusBar.setBleStatus(BleStatus::ADVERTISING);

        // Inicializa fila de eventos de configuracao
        config_event_queue_init();
    }

    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "Sistema Pronto! (v2 Screen Manager)");
    ESP_LOGI(TAG, "- Tela inicial: Numpad");
    ESP_LOGI(TAG, "- Menu: Navega para Jornada");
    ESP_LOGI(TAG, "- Voltar: Retorna tela anterior");
    ESP_LOGI(TAG, "- BLE: Advertising ativo");
    ESP_LOGI(TAG, "=================================");

    // Loop principal
    uint32_t lastUpdate = 0;

    while (1) {
        uint32_t now = time_millis();

        // Atualizacao periodica (1 segundo) - StatusBar com dados de ignicao
        if ((now - lastUpdate) >= 1000) {
            lastUpdate = now;

            bool ignicaoOn = getIgnicaoStatus();
            uint32_t tempoIgnicao = (ignicaoLigada && ignicaoOn) ? (now - ignicaoStartTime) : 0;
            StatusBarData data = {
                .ignicaoOn = ignicaoOn,
                .tempoIgnicao = tempoIgnicao,
                .tempoJornada = 0,
                .mensagem = nullptr
            };
            statusBar.update(data);
        }

        // Processa eventos BLE (ponte thread-safe NimBLE -> UI)
        ble_process_events(onBleEvent);

        // Processa eventos de configuracao BLE
        config_process_events(onConfigEvent);

        // Atualiza tela atual via ScreenManager
        if (screenMgr) {
            screenMgr->update();
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
    // Imprime informacoes de versao
    app_print_version();

    // Inicializa filesystem
    if (!app_init_filesystem()) {
        ESP_LOGE(TAG, "ERRO: Falha ao montar LittleFS!");
        return;
    }

    // Inicializa display
    ESP_LOGI(TAG, "Inicializando display...");

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = DISPLAY_BUFFER_SIZE,
        .rotate = LV_DISP_ROT_90,
    };

    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    // Inicializa driver de filesystem LVGL
    lvgl_fs_init('A');

    // Inicializa decoder PNG
    lv_png_init();

    ESP_LOGI(TAG, "Display inicializado!");

    // Exibe splash screen
    ESP_LOGI(TAG, "Exibindo splash screen...");
    createSplashScreen();

    // Cria task principal
    xTaskCreatePinnedToCore(
        system_task,
        "system_task",
        SYSTEM_TASK_STACK_SIZE,
        NULL,
        SYSTEM_TASK_PRIORITY,
        NULL,
        SYSTEM_TASK_CORE
    );
}
