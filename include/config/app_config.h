/**
 * ============================================================================
 * CONFIGURACAO CENTRALIZADA DO SISTEMA
 * ============================================================================
 *
 * Teclado de Jornada Digital
 * Copyright (c) 2024-2026 Getscale Sistemas Embarcados
 * Desenvolvido por Mario Stanski Jr
 *
 * Este arquivo centraliza todas as configuracoes do sistema.
 * Modifique os valores aqui para ajustar o comportamento global.
 *
 * ============================================================================
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// VERSAO DO SISTEMA
// ============================================================================

#define APP_VERSION_MAJOR       1
#define APP_VERSION_MINOR       0
#define APP_VERSION_PATCH       0
#define APP_VERSION_STRING      "1.0.0"
#define APP_NAME                "Teclado de Jornada Digital"
#define APP_AUTHOR              "Mario Stanski Jr"
#define APP_COMPANY             "Getscale Sistemas Embarcados"

// ============================================================================
// CONFIGURACOES DE DISPLAY
// ============================================================================

#define DISPLAY_WIDTH           480
#define DISPLAY_HEIGHT          320
#define DISPLAY_ROTATION        90      // Rotacao em graus (0, 90, 180, 270)
#define DISPLAY_COLOR_DEPTH     16      // Bits por pixel

// Buffer de display
#define DISPLAY_BUFFER_SIZE     (DISPLAY_WIDTH * DISPLAY_HEIGHT)

// ============================================================================
// CONFIGURACOES DA GRADE DE BOTOES
// ============================================================================

#define GRID_COLS               4
#define GRID_ROWS               3
#define GRID_BUTTON_WIDTH       110
#define GRID_BUTTON_HEIGHT      85
#define GRID_BUTTON_MARGIN      5
#define GRID_PADDING            10
#define GRID_TOTAL_BUTTONS      (GRID_COLS * GRID_ROWS)

// ============================================================================
// CONFIGURACOES DA BARRA DE STATUS
// ============================================================================

#define STATUS_BAR_HEIGHT       40
#define STATUS_BAR_UPDATE_MS    250     // Intervalo de atualizacao
#define STATUS_MESSAGE_TIMEOUT  3000    // Timeout padrao de mensagens

// Area util da grade (sem barra de status)
#define GRID_AREA_HEIGHT        (DISPLAY_HEIGHT - STATUS_BAR_HEIGHT)

// ============================================================================
// CONFIGURACOES DE JORNADA
// ============================================================================

#define MAX_MOTORISTAS          3
#define MAX_NOME_MOTORISTA      32      // Tamanho maximo do nome
#define NUM_ESTADOS_JORNADA     7       // Numero de estados possiveis

// Estados de jornada (para referencia)
// 0 = INATIVO
// 1 = JORNADA (direcao)
// 2 = MANOBRA
// 3 = REFEICAO
// 4 = ESPERA
// 5 = DESCARGA
// 6 = ABASTECIMENTO

// ============================================================================
// CONFIGURACOES DE IGNICAO
// ============================================================================

#define IGNICAO_PIN             18
#define IGNICAO_DEBOUNCE_ON_S   1.0f    // Segundos para confirmar ON
#define IGNICAO_DEBOUNCE_OFF_S  2.0f    // Segundos para confirmar OFF
#define IGNICAO_CHECK_INTERVAL  100     // Intervalo de verificacao (ms)
#define IGNICAO_MIN_DEBOUNCE    0.0f    // Debounce minimo
#define IGNICAO_MAX_DEBOUNCE    10.0f   // Debounce maximo

// ============================================================================
// CONFIGURACOES DE AUDIO
// ============================================================================

#define AUDIO_TASK_CORE         1       // Core dedicado para audio
#define AUDIO_TASK_PRIORITY     5       // Prioridade da task
#define AUDIO_TASK_STACK_SIZE   (32 * 1024)  // 32KB de stack

#define AUDIO_BUFFER_SIZE       (8 * 1024)   // Buffer de leitura MP3
#define AUDIO_PCM_BUFFER_SIZE   MINIMP3_MAX_SAMPLES_PER_FRAME
#define AUDIO_QUEUE_SIZE        4       // Tamanho da fila de requisicoes
#define AUDIO_I2S_TIMEOUT_MS    100     // Timeout de escrita I2S

#define AUDIO_VOLUME_MIN        0
#define AUDIO_VOLUME_MAX        21
#define AUDIO_VOLUME_DEFAULT    21      // Volume inicial (maximo)

// Pinos I2S
#define AUDIO_I2S_PORT          I2S_NUM_0
#define AUDIO_I2S_MCK_IO        (-1)
#define AUDIO_I2S_BCK_IO        42
#define AUDIO_I2S_LRCK_IO       2
#define AUDIO_I2S_DO_IO         41

// ============================================================================
// CONFIGURACOES DE PINOS - DISPLAY QSPI
// ============================================================================

#define TFT_CS                  45
#define TFT_SCK                 47
#define TFT_SDA0                21
#define TFT_SDA1                48
#define TFT_SDA2                40
#define TFT_SDA3                39
#define TFT_TE                  38
#define TFT_BLK                 1
#define TFT_RST                 (-1)
#define TFT_BLK_ON_LEVEL        1

// ============================================================================
// CONFIGURACOES DE PINOS - TOUCH I2C
// ============================================================================

#define TOUCH_I2C_SCL           8
#define TOUCH_I2C_SDA           4
#define TOUCH_INT               3
#define TOUCH_RST               (-1)

// ============================================================================
// CONFIGURACOES DE PINOS - SD CARD
// ============================================================================

#define SD_MMC_D0               13
#define SD_MMC_CLK              12
#define SD_MMC_CMD              11

// ============================================================================
// CONFIGURACOES DE PINOS - OUTROS
// ============================================================================

#define BAT_ADC_PIN             5

// ============================================================================
// CONFIGURACOES DE TIMEOUT
// ============================================================================

#define NUMPAD_TIMEOUT_MS       10000   // Timeout do teclado numerico
#define POPUP_DEFAULT_TIMEOUT   3000    // Timeout padrao de popups
#define BUTTON_DEBOUNCE_MS      300     // Debounce de botoes UI
#define DISPLAY_LOCK_TIMEOUT    100     // Timeout para lock do display
#define MUTEX_WAIT_TIMEOUT      portMAX_DELAY

// ============================================================================
// CONFIGURACOES DE RETRY
// ============================================================================

#define BUTTON_MAX_RETRY        3       // Tentativas maximas de criar botao
#define BUTTON_RETRY_DELAY_MS   100     // Delay entre tentativas

// ============================================================================
// CONFIGURACOES DE FILESYSTEM
// ============================================================================

#define FS_BASE_PATH            "/littlefs"
#define FS_PARTITION_LABEL      "spiffs"
#define FS_MAX_FILES            5
#define FS_FORMAT_IF_FAILED     false

// ============================================================================
// CONFIGURACOES DE DEBUG
// ============================================================================

#define DEBUG_ENABLED           1
#define DEBUG_LOG_LEVEL         ESP_LOG_INFO
#define DEBUG_PRINT_GRID        0       // Imprimir ocupacao da grade

// ============================================================================
// CONFIGURACOES DE TASKS
// ============================================================================

#define SYSTEM_TASK_CORE        0       // Core da task principal
#define SYSTEM_TASK_PRIORITY    5       // Prioridade da task principal
#define SYSTEM_TASK_STACK_SIZE  8192    // Stack da task principal

#define IGNICAO_TASK_CORE       0
#define IGNICAO_TASK_PRIORITY   2
#define IGNICAO_TASK_STACK_SIZE 4096

// ============================================================================
// CORES DO TEMA (formato 0xRRGGBB)
// ============================================================================

// Cores de fundo
#define THEME_BG_PRIMARY        0x1a1a1a
#define THEME_BG_SECONDARY      0x2a2a2a
#define THEME_BG_TERTIARY       0x3a3a3a

// Cores de status
#define THEME_COLOR_SUCCESS     0x00AA00
#define THEME_COLOR_WARNING     0xFFAA00
#define THEME_COLOR_ERROR       0xFF0000
#define THEME_COLOR_INFO        0x0088FF

// Cores de texto
#define THEME_TEXT_PRIMARY      0xFFFFFF
#define THEME_TEXT_SECONDARY    0xCCCCCC
#define THEME_TEXT_MUTED        0x888888

// Cores de botoes por estado de jornada
#define THEME_BTN_JORNADA       0x00AA00    // Verde
#define THEME_BTN_MANOBRA       0x0088FF    // Azul
#define THEME_BTN_REFEICAO      0x0088FF    // Azul
#define THEME_BTN_ESPERA        0x0088FF    // Azul
#define THEME_BTN_CARGA         0xFF8800    // Laranja
#define THEME_BTN_DESCARGA      0xFF8800    // Laranja
#define THEME_BTN_ABASTECER     0x0088FF    // Azul
#define THEME_BTN_DESCANSAR     0x0088FF    // Azul
#define THEME_BTN_EMERGENCIA    0xFF0000    // Vermelho
#define THEME_BTN_NUMPAD        0x4444FF    // Azul claro

// ============================================================================
// ARQUIVOS DE AUDIO
// ============================================================================

#define AUDIO_FILE_IGN_ON       "/ign_on_jornada_manobra.mp3"
#define AUDIO_FILE_IGN_OFF      "/ign_off.mp3"
#define AUDIO_FILE_CLICK        "/click.mp3"
#define AUDIO_FILE_OK           "/ok_click.mp3"
#define AUDIO_FILE_NOK          "/nok_click.mp3"
#define AUDIO_FILE_NOK_USER     "/nok_user.mp3"
#define AUDIO_FILE_ID_OK        "/identificacao_ok.mp3"
#define AUDIO_FILE_SENHA        "/digite_senha.mp3"
#define AUDIO_FILE_RFID         "/aproxime_rfid.mp3"
#define AUDIO_FILE_ALERT_RPM    "/alerta_max_rpm.mp3"
#define AUDIO_FILE_ALERT_VEL    "/alerta_max_vel.mp3"

// ============================================================================
// MACROS UTILITARIAS
// ============================================================================

// Converte segundos para milissegundos
#define SECONDS_TO_MS(s)        ((uint32_t)((s) * 1000))

// Converte milissegundos para segundos
#define MS_TO_SECONDS(ms)       ((float)(ms) / 1000.0f)

// Verifica se valor esta em range
#define IN_RANGE(val, min, max) ((val) >= (min) && (val) <= (max))

// Limita valor a um range
#define CLAMP(val, min, max)    ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

// Tamanho de array
#define ARRAY_SIZE(arr)         (sizeof(arr) / sizeof((arr)[0]))

#ifdef __cplusplus
}
#endif

#endif // APP_CONFIG_H
