/**
 * ============================================================================
 * GATT JOURNEY SERVICE - IMPLEMENTACAO
 * ============================================================================
 *
 * Callbacks de acesso e funcoes de empacotamento para as caracteristicas
 * Journey State e Ignition Status do servico BLE Journey.
 *
 * Journey State: 3 motoristas * 8 bytes = 24 bytes (packed struct)
 * Ignition Status: 8 bytes (packed struct)
 *
 * ============================================================================
 */

#include "services/ble/gatt/gatt_journey.h"
#include "services/jornada/jornada_service.h"
#include "services/ignicao/ignicao_service.h"
#include "config/app_config.h"
#include "utils/time_utils.h"

// NimBLE
#include "host/ble_hs.h"
#include "os/os_mbuf.h"

// ESP-IDF
#include "esp_log.h"

#include <string.h>

// ============================================================================
// TAG DE LOG
// ============================================================================

static const char* TAG = "GATT_JOURNEY";

// ============================================================================
// EMPACOTAMENTO DE DADOS
// ============================================================================

int pack_journey_states(uint8_t* buf, size_t buf_len) {
    const size_t required = sizeof(JourneyStateData) * MAX_MOTORISTAS;

    if (buf == NULL || buf_len < required) {
        ESP_LOGE(TAG, "Buffer insuficiente: %zu < %zu", buf_len, required);
        return -1;
    }

    JornadaService* svc = JornadaService::getInstance();
    JourneyStateData* data = reinterpret_cast<JourneyStateData*>(buf);

    for (int i = 0; i < MAX_MOTORISTAS; i++) {
        int motorist_id = i + 1;
        DadosMotorista dados;
        memset(&data[i], 0, sizeof(JourneyStateData));
        data[i].motorist_id = static_cast<uint8_t>(motorist_id);

        if (svc->getMotorista(motorist_id, &dados)) {
            data[i].state = static_cast<uint8_t>(dados.estadoAtual);
            data[i].active = dados.ativo ? 1 : 0;
            data[i].time_in_state = svc->getTempoEstadoAtual(motorist_id);
        }
        // Se getMotorista falhar, os campos ficam zerados (motorist_id ja setado)
    }

    ESP_LOGD(TAG, "Journey states empacotados: %zu bytes", required);
    return static_cast<int>(required);
}

int pack_ignition_data(uint8_t* buf, size_t buf_len) {
    const size_t required = sizeof(IgnitionData);

    if (buf == NULL || buf_len < required) {
        ESP_LOGE(TAG, "Buffer insuficiente: %zu < %zu", buf_len, required);
        return -1;
    }

    IgnitionData* data = reinterpret_cast<IgnitionData*>(buf);
    memset(data, 0, sizeof(IgnitionData));

    IgnicaoService* ignicao = IgnicaoService::getInstance();
    data->status = ignicao->getStatus() ? 1 : 0;

    // Calcula duracao no estado atual usando stats
    IgnicaoStats stats = ignicao->getStats();
    if (stats.lastChangeTime > 0) {
        data->duration_ms = time_elapsed_since(stats.lastChangeTime);
    }

    ESP_LOGD(TAG, "Ignition data empacotado: status=%d, duration=%lu ms",
             data->status, (unsigned long)data->duration_ms);
    return static_cast<int>(required);
}

// ============================================================================
// CALLBACKS DE ACESSO GATT
// ============================================================================

int journey_state_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t buf[sizeof(JourneyStateData) * MAX_MOTORISTAS];
        int len = pack_journey_states(buf, sizeof(buf));

        if (len < 0) {
            ESP_LOGE(TAG, "Falha ao empacotar journey states");
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        int rc = os_mbuf_append(ctxt->om, buf, static_cast<uint16_t>(len));
        if (rc != 0) {
            ESP_LOGE(TAG, "Falha ao anexar journey states ao mbuf: %d", rc);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        ESP_LOGD(TAG, "Journey state lido: %d bytes", len);
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

int ignition_status_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t buf[sizeof(IgnitionData)];
        int len = pack_ignition_data(buf, sizeof(buf));

        if (len < 0) {
            ESP_LOGE(TAG, "Falha ao empacotar ignition data");
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        int rc = os_mbuf_append(ctxt->om, buf, static_cast<uint16_t>(len));
        if (rc != 0) {
            ESP_LOGE(TAG, "Falha ao anexar ignition data ao mbuf: %d", rc);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        ESP_LOGD(TAG, "Ignition status lido: %d bytes", len);
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}
