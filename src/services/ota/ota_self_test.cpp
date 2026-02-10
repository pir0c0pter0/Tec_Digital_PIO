/**
 * ============================================================================
 * SELF-TEST POS-OTA - IMPLEMENTACAO
 * ============================================================================
 *
 * Executa diagnostico de subsistemas criticos no primeiro boot apos OTA.
 * Se todos passarem, marca firmware valido. Se qualquer falhar, reverte.
 *
 * Protegido por task watchdog (60s): se self-test travar, watchdog reinicia
 * o dispositivo e bootloader reverte para firmware anterior (PENDING_VERIFY
 * nunca confirmado).
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "services/ota/ota_self_test.h"
#include "config/app_config.h"

#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "lvgl.h"

#include "services/nvs/nvs_manager.h"
#include "services/ble/ble_service.h"
#include "simple_audio_manager.h"

static const char* TAG = "OTA_TEST";

// ============================================================================
// FUNCAO DE SELF-TEST
// ============================================================================

void ota_self_test(void) {
    // 1. Obtem particao em execucao
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGW(TAG, "Nao foi possivel obter particao em execucao");
        return;
    }

    // 2. Verifica estado OTA da particao
    esp_ota_img_states_t ota_state;
    esp_err_t err = esp_ota_get_state_partition(running, &ota_state);
    if (err != ESP_OK) {
        // Particao pode nao ter estado OTA (factory boot, primeira vez)
        ESP_LOGI(TAG, "Sem estado OTA na particao (err=%d) -- boot normal", (int)err);
        return;
    }

    // 3. Se nao esta PENDING_VERIFY, nao eh primeiro boot pos-OTA
    if (ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Firmware ja confirmado (state=%d) -- boot normal", (int)ota_state);
        return;
    }

    // ========================================================================
    // PRIMEIRO BOOT POS-OTA -- SELF-TEST
    // ========================================================================

    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "Primeiro boot apos OTA -- executando self-test...");
    ESP_LOGW(TAG, "========================================");

    // Configura task watchdog para proteger contra travamento
    // Se self-test travar, watchdog reinicia e bootloader reverte
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = OTA_SELF_TEST_TIMEOUT_S * 1000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_err_t wdt_err = esp_task_wdt_reconfigure(&wdt_config);
    if (wdt_err != ESP_OK) {
        ESP_LOGW(TAG, "Falha ao reconfigurar task watchdog (err=%d)", (int)wdt_err);
    }

    // Subscreve task atual no watchdog
    esp_task_wdt_add(NULL);

    bool allPassed = true;
    int testsPassed = 0;
    int testsTotal = 4;

    // ========================================================================
    // TESTE 1: LVGL Display
    // ========================================================================

    ESP_LOGI(TAG, "[1/%d] Testando LVGL display...", testsTotal);
    if (lv_disp_get_default() != NULL) {
        ESP_LOGI(TAG, "  LVGL display: OK");
        testsPassed++;
    } else {
        ESP_LOGE(TAG, "  LVGL display: FALHOU (display nao inicializado)");
        allPassed = false;
    }
    esp_task_wdt_reset();

    // ========================================================================
    // TESTE 2: NVS
    // ========================================================================

    ESP_LOGI(TAG, "[2/%d] Testando NVS...", testsTotal);
    auto* nvsMgr = NvsManager::getInstance();
    if (nvsMgr->init()) {
        ESP_LOGI(TAG, "  NVS: OK");
        testsPassed++;
    } else {
        ESP_LOGE(TAG, "  NVS: FALHOU");
        allPassed = false;
    }
    esp_task_wdt_reset();

    // ========================================================================
    // TESTE 3: BLE
    // ========================================================================

    ESP_LOGI(TAG, "[3/%d] Testando BLE...", testsTotal);
    auto* bleSvc = BleService::getInstance();
    if (bleSvc->init()) {
        ESP_LOGI(TAG, "  BLE: OK");
        testsPassed++;
    } else {
        ESP_LOGE(TAG, "  BLE: FALHOU");
        allPassed = false;
    }
    esp_task_wdt_reset();

    // ========================================================================
    // TESTE 4: Audio
    // ========================================================================

    ESP_LOGI(TAG, "[4/%d] Testando audio...", testsTotal);
    // initSimpleAudio() eh idempotente -- retorna se ja inicializado
    // Como nao retorna bool, verificamos que nao causa crash
    initSimpleAudio();
    ESP_LOGI(TAG, "  Audio: OK (init completou sem crash)");
    testsPassed++;
    esp_task_wdt_reset();

    // ========================================================================
    // RESULTADO
    // ========================================================================

    ESP_LOGI(TAG, "Self-test: %d/%d testes passaram", testsPassed, testsTotal);

    if (allPassed) {
        // Marca firmware como valido -- cancela rollback
        err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            ESP_LOGW(TAG, "========================================");
            ESP_LOGW(TAG, "Self-test APROVADO -- firmware confirmado!");
            ESP_LOGW(TAG, "========================================");
        } else {
            ESP_LOGE(TAG, "Falha ao marcar firmware valido (err=%d)", (int)err);
            // Continua mesmo assim -- melhor que reverter se testes passaram
        }
    } else {
        ESP_LOGE(TAG, "========================================");
        ESP_LOGE(TAG, "Self-test FALHOU -- revertendo para firmware anterior!");
        ESP_LOGE(TAG, "========================================");
        // Marca invalido e reinicia imediatamente
        // Bootloader carrega particao anterior (que ja eh VALID)
        esp_ota_mark_app_invalid_rollback_and_reboot();
        // Nao retorna -- dispositivo reinicia
    }

    // Remove task do watchdog (self-test concluido)
    esp_task_wdt_delete(NULL);

    // Restaura configuracao padrao do watchdog
    esp_task_wdt_config_t default_config = {
        .timeout_ms = 5000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&default_config);
}
