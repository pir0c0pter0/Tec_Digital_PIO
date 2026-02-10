/**
 * ============================================================================
 * GATT WRITE VALIDATION UTILITY - HEADER
 * ============================================================================
 *
 * Funcoes utilitarias para validacao de escritas GATT do lado do servidor.
 * Verifica bounds, tipo de operacao e tamanho de dados antes de processar.
 *
 * Scaffold para Phase 3 (Settings + Config Sync) -- nenhuma caracteristica
 * escrevivel existe em Phase 2. Uso futuro em Configuration Service.
 *
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * ============================================================================
 */

#ifndef GATT_VALIDATION_H
#define GATT_VALIDATION_H

#include <stdint.h>
#include <stddef.h>

// NimBLE headers necessarios para os tipos e macros usados
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "os/os_mbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Valida dados de escrita GATT (bounds checking, tipo, tamanho).
 *
 * Deve ser chamado no inicio de callbacks de acesso para caracteristicas
 * escreviveis. Retorna 0 se valido, codigo de erro ATT se invalido.
 *
 * @param ctxt Contexto de acesso GATT (do callback NimBLE)
 * @param min_len Tamanho minimo esperado dos dados
 * @param max_len Tamanho maximo esperado dos dados
 * @return 0 se valido, BLE_ATT_ERR_* se invalido
 */
static inline int gatt_validate_write(struct ble_gatt_access_ctxt *ctxt,
                                       size_t min_len, size_t max_len) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < min_len || len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    return 0;
}

/**
 * Extrai dados de escrita de um mbuf para buffer linear.
 *
 * Copia os dados do mbuf chain para um buffer flat para processamento.
 * Deve ser chamado apos gatt_validate_write() confirmar tamanho valido.
 *
 * @param ctxt Contexto de acesso GATT
 * @param buf Buffer de destino
 * @param buf_len Tamanho do buffer de destino
 * @return Numero de bytes lidos, ou -1 em erro
 */
static inline int gatt_read_write_data(struct ble_gatt_access_ctxt *ctxt,
                                        uint8_t* buf, size_t buf_len) {
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > buf_len) return -1;

    uint16_t out_len = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, buf_len, &out_len);
    return (rc == 0) ? (int)out_len : -1;
}

#ifdef __cplusplus
}
#endif

#endif // GATT_VALIDATION_H
