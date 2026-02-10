/**
 * ============================================================================
 * NVS MANAGER - IMPLEMENTACAO
 * ============================================================================
 *
 * Persistencia de dados usando particao NVS dedicada (nvs_data).
 * Separa dados da aplicacao do NVS do sistema.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#include "services/nvs/nvs_manager.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "NVS_MGR";

// ============================================================================
// SINGLETON
// ============================================================================

NvsManager* NvsManager::instance_ = nullptr;

NvsManager::NvsManager()
    : mutex_(nullptr)
    , initialized_(false)
{
}

NvsManager::~NvsManager() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

NvsManager* NvsManager::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new NvsManager();
    }
    return instance_;
}

// ============================================================================
// INICIALIZACAO
// ============================================================================

bool NvsManager::init() {
    if (initialized_) {
        ESP_LOGW(TAG, "NVS ja inicializado");
        return true;
    }

    // Cria mutex para thread safety
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "Falha ao criar mutex NVS");
        return false;
    }

    // Inicializa particao dedicada (nvs_data)
    ESP_LOGI(TAG, "Inicializando particao NVS: %s", NVS_PARTITION_LABEL);
    esp_err_t err = nvs_flash_init_partition(NVS_PARTITION_LABEL);

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrompido ou versao nova, apagando particao...");
        err = nvs_flash_erase_partition(NVS_PARTITION_LABEL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Falha ao apagar particao NVS: %s", esp_err_to_name(err));
            return false;
        }
        err = nvs_flash_init_partition(NVS_PARTITION_LABEL);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar NVS: %s", esp_err_to_name(err));
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "NVS inicializado com sucesso (particao: %s)", NVS_PARTITION_LABEL);
    return true;
}

// ============================================================================
// HELPER: ABRIR HANDLE
// ============================================================================

bool NvsManager::openHandle(const char* ns, nvs_open_mode_t mode, nvs_handle_t* handle) {
    esp_err_t err = nvs_open_from_partition(NVS_PARTITION_LABEL, ns, mode, handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao abrir namespace '%s': %s", ns, esp_err_to_name(err));
        return false;
    }
    return true;
}

// ============================================================================
// CONFIGURACOES: VOLUME
// ============================================================================

bool NvsManager::saveVolume(uint8_t volume) {
    if (!initialized_) return false;

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return false;

    nvs_handle_t handle;
    bool ok = false;

    if (openHandle(NVS_NS_SETTINGS, NVS_READWRITE, &handle)) {
        esp_err_t err = nvs_set_u8(handle, NVS_KEY_VOLUME, volume);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
            ok = (err == ESP_OK);
        }
        if (!ok) {
            ESP_LOGE(TAG, "Falha ao salvar volume: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Volume salvo: %d", volume);
        }
        nvs_close(handle);
    }

    xSemaphoreGive(mutex_);
    return ok;
}

uint8_t NvsManager::loadVolume(uint8_t defaultVal) {
    if (!initialized_) return defaultVal;

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return defaultVal;

    nvs_handle_t handle;
    uint8_t value = defaultVal;

    if (openHandle(NVS_NS_SETTINGS, NVS_READONLY, &handle)) {
        esp_err_t err = nvs_get_u8(handle, NVS_KEY_VOLUME, &value);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "Volume nao encontrado no NVS, usando padrao: %d", defaultVal);
            value = defaultVal;
        } else if (err != ESP_OK) {
            ESP_LOGW(TAG, "Erro ao ler volume: %s", esp_err_to_name(err));
            value = defaultVal;
        }
        nvs_close(handle);
    }

    xSemaphoreGive(mutex_);
    return value;
}

// ============================================================================
// CONFIGURACOES: BRILHO
// ============================================================================

bool NvsManager::saveBrightness(uint8_t brightness) {
    if (!initialized_) return false;

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return false;

    nvs_handle_t handle;
    bool ok = false;

    if (openHandle(NVS_NS_SETTINGS, NVS_READWRITE, &handle)) {
        esp_err_t err = nvs_set_u8(handle, NVS_KEY_BRIGHTNESS, brightness);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
            ok = (err == ESP_OK);
        }
        if (!ok) {
            ESP_LOGE(TAG, "Falha ao salvar brilho: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Brilho salvo: %d%%", brightness);
        }
        nvs_close(handle);
    }

    xSemaphoreGive(mutex_);
    return ok;
}

uint8_t NvsManager::loadBrightness(uint8_t defaultVal) {
    if (!initialized_) return defaultVal;

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return defaultVal;

    nvs_handle_t handle;
    uint8_t value = defaultVal;

    if (openHandle(NVS_NS_SETTINGS, NVS_READONLY, &handle)) {
        esp_err_t err = nvs_get_u8(handle, NVS_KEY_BRIGHTNESS, &value);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "Brilho nao encontrado no NVS, usando padrao: %d%%", defaultVal);
            value = defaultVal;
        } else if (err != ESP_OK) {
            ESP_LOGW(TAG, "Erro ao ler brilho: %s", esp_err_to_name(err));
            value = defaultVal;
        }
        nvs_close(handle);
    }

    xSemaphoreGive(mutex_);
    return value;
}

// ============================================================================
// ESTADO DE JORNADA POR MOTORISTA
// ============================================================================

bool NvsManager::saveJornadaState(uint8_t motoristId, const void* state, size_t size) {
    if (!initialized_ || !state) return false;
    if (motoristId >= MAX_MOTORISTAS) {
        ESP_LOGE(TAG, "motoristId invalido: %d (max: %d)", motoristId, MAX_MOTORISTAS - 1);
        return false;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return false;

    nvs_handle_t handle;
    bool ok = false;

    if (openHandle(NVS_NS_JORNADA, NVS_READWRITE, &handle)) {
        // Chave: "mot_X" onde X = motoristId (0-2)
        char key[8];
        snprintf(key, sizeof(key), "mot_%d", motoristId);

        esp_err_t err = nvs_set_blob(handle, key, state, size);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
            ok = (err == ESP_OK);
        }
        if (!ok) {
            ESP_LOGE(TAG, "Falha ao salvar jornada mot_%d: %s", motoristId, esp_err_to_name(err));
        } else {
            ESP_LOGD(TAG, "Jornada mot_%d salva (%d bytes)", motoristId, size);
        }
        nvs_close(handle);
    }

    xSemaphoreGive(mutex_);
    return ok;
}

bool NvsManager::loadJornadaState(uint8_t motoristId, void* state, size_t size) {
    if (!initialized_ || !state) return false;
    if (motoristId >= MAX_MOTORISTAS) {
        ESP_LOGE(TAG, "motoristId invalido: %d (max: %d)", motoristId, MAX_MOTORISTAS - 1);
        return false;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return false;

    nvs_handle_t handle;
    bool ok = false;

    if (openHandle(NVS_NS_JORNADA, NVS_READONLY, &handle)) {
        char key[8];
        snprintf(key, sizeof(key), "mot_%d", motoristId);

        size_t storedSize = size;
        esp_err_t err = nvs_get_blob(handle, key, state, &storedSize);

        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "Jornada mot_%d nao encontrada no NVS", motoristId);
        } else if (err != ESP_OK) {
            ESP_LOGW(TAG, "Erro ao ler jornada mot_%d: %s", motoristId, esp_err_to_name(err));
        } else if (storedSize != size) {
            ESP_LOGW(TAG, "Jornada mot_%d: tamanho incompativel (esperado=%d, lido=%d)",
                     motoristId, size, storedSize);
        } else {
            // Valida versao da struct
            const NvsJornadaState* jState = static_cast<const NvsJornadaState*>(state);
            if (jState->version != NVS_JORNADA_VERSION) {
                ESP_LOGW(TAG, "Jornada mot_%d: versao incompativel (esperada=%d, lida=%d)",
                         motoristId, NVS_JORNADA_VERSION, jState->version);
            } else {
                ok = true;
                ESP_LOGD(TAG, "Jornada mot_%d restaurada (estado=%d, ativo=%d)",
                         motoristId, jState->estadoAtual, jState->ativo);
            }
        }
        nvs_close(handle);
    }

    xSemaphoreGive(mutex_);
    return ok;
}

bool NvsManager::clearJornadaState(uint8_t motoristId) {
    if (!initialized_) return false;
    if (motoristId >= MAX_MOTORISTAS) return false;

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return false;

    nvs_handle_t handle;
    bool ok = false;

    if (openHandle(NVS_NS_JORNADA, NVS_READWRITE, &handle)) {
        char key[8];
        snprintf(key, sizeof(key), "mot_%d", motoristId);

        esp_err_t err = nvs_erase_key(handle, key);
        if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
            nvs_commit(handle);
            ok = true;
            ESP_LOGI(TAG, "Jornada mot_%d limpa", motoristId);
        } else {
            ESP_LOGE(TAG, "Falha ao limpar jornada mot_%d: %s", motoristId, esp_err_to_name(err));
        }
        nvs_close(handle);
    }

    xSemaphoreGive(mutex_);
    return ok;
}

// ============================================================================
// NOMES DE MOTORISTAS
// ============================================================================

bool NvsManager::saveDriverName(uint8_t driverId, const char* name) {
    if (!initialized_ || !name) return false;
    if (driverId >= MAX_MOTORISTAS) {
        ESP_LOGE(TAG, "driverId invalido: %d (max: %d)", driverId, MAX_MOTORISTAS - 1);
        return false;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return false;

    nvs_handle_t handle;
    bool ok = false;

    if (openHandle(NVS_NS_SETTINGS, NVS_READWRITE, &handle)) {
        // Chave: "drv_X" onde X = driverId (0-2)
        char key[8];
        snprintf(key, sizeof(key), NVS_KEY_DRIVER_PREFIX "%d", driverId);

        esp_err_t err = nvs_set_str(handle, key, name);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
            ok = (err == ESP_OK);
        }
        if (!ok) {
            ESP_LOGE(TAG, "Falha ao salvar nome motorista %d: %s", driverId, esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Nome motorista %d salvo: '%s'", driverId, name);
        }
        nvs_close(handle);
    }

    xSemaphoreGive(mutex_);
    return ok;
}

bool NvsManager::loadDriverName(uint8_t driverId, char* name, size_t maxLen) {
    if (!initialized_ || !name || maxLen == 0) return false;
    if (driverId >= MAX_MOTORISTAS) {
        ESP_LOGE(TAG, "driverId invalido: %d (max: %d)", driverId, MAX_MOTORISTAS - 1);
        return false;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return false;

    nvs_handle_t handle;
    bool ok = false;

    if (openHandle(NVS_NS_SETTINGS, NVS_READONLY, &handle)) {
        char key[8];
        snprintf(key, sizeof(key), NVS_KEY_DRIVER_PREFIX "%d", driverId);

        size_t requiredLen = maxLen;
        esp_err_t err = nvs_get_str(handle, key, name, &requiredLen);

        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "Nome motorista %d nao encontrado no NVS", driverId);
            name[0] = '\0';
        } else if (err != ESP_OK) {
            ESP_LOGW(TAG, "Erro ao ler nome motorista %d: %s", driverId, esp_err_to_name(err));
            name[0] = '\0';
        } else {
            ok = true;
            ESP_LOGD(TAG, "Nome motorista %d carregado: '%s'", driverId, name);
        }
        nvs_close(handle);
    }

    xSemaphoreGive(mutex_);
    return ok;
}

// ============================================================================
// INTERFACE C
// ============================================================================

extern "C" {

bool nvs_manager_init(void) {
    return NvsManager::getInstance()->init();
}

bool nvs_manager_save_volume(uint8_t volume) {
    return NvsManager::getInstance()->saveVolume(volume);
}

uint8_t nvs_manager_load_volume(uint8_t default_val) {
    return NvsManager::getInstance()->loadVolume(default_val);
}

bool nvs_manager_save_brightness(uint8_t brightness) {
    return NvsManager::getInstance()->saveBrightness(brightness);
}

uint8_t nvs_manager_load_brightness(uint8_t default_val) {
    return NvsManager::getInstance()->loadBrightness(default_val);
}

bool nvs_manager_save_driver_name(uint8_t driver_id, const char* name) {
    return NvsManager::getInstance()->saveDriverName(driver_id, name);
}

bool nvs_manager_load_driver_name(uint8_t driver_id, char* name, size_t max_len) {
    return NvsManager::getInstance()->loadDriverName(driver_id, name, max_len);
}

} // extern "C"
