/**
 * ============================================================================
 * CAMADA DE COMPATIBILIDADE ARDUINO -> ESP-IDF
 * ============================================================================
 *
 * Fornece macros e funções para facilitar migração de código Arduino para
 * ESP-IDF puro, mantendo a sintaxe familiar.
 *
 * ============================================================================
 */

#ifndef ARDUINO_COMPAT_H
#define ARDUINO_COMPAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TIMING FUNCTIONS
// ============================================================================

/**
 * Retorna o tempo em milissegundos desde o boot
 */
static inline uint32_t millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/**
 * Retorna o tempo em microssegundos desde o boot
 */
static inline uint32_t micros(void) {
    return (uint32_t)esp_timer_get_time();
}

/**
 * Delay em milissegundos (blocking)
 */
static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * Delay em microssegundos (busy wait para delays curtos)
 */
static inline void delayMicroseconds(uint32_t us) {
    uint32_t start = micros();
    while ((micros() - start) < us) {
        // Busy wait
    }
}

// ============================================================================
// GPIO DEFINITIONS
// ============================================================================

#define INPUT           GPIO_MODE_INPUT
#define OUTPUT          GPIO_MODE_OUTPUT
#define INPUT_PULLUP    GPIO_MODE_INPUT
#define INPUT_PULLDOWN  GPIO_MODE_INPUT

#define HIGH            1
#define LOW             0

/**
 * Configura modo do pino GPIO
 */
static inline void pinMode(int pin, int mode) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = (gpio_mode_t)mode,
        .pull_up_en = (mode == INPUT_PULLUP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

/**
 * Escreve valor no pino GPIO
 */
static inline void digitalWrite(int pin, int value) {
    gpio_set_level((gpio_num_t)pin, value);
}

/**
 * Lê valor do pino GPIO
 */
static inline int digitalRead(int pin) {
    return gpio_get_level((gpio_num_t)pin);
}

// ============================================================================
// SERIAL REPLACEMENT -> ESP_LOG
// ============================================================================

// Tag padrão para logs
#ifndef LOG_TAG
#define LOG_TAG "APP"
#endif

// Classe wrapper para Serial (C++ only)
#ifdef __cplusplus

class SerialClass {
public:
    void begin(unsigned long baud) {
        // Não faz nada - logs via ESP_LOG não precisam inicialização
        ESP_LOGI(LOG_TAG, "Serial initialized (baud ignored in ESP-IDF)");
    }

    void print(const char* str) {
        esp_rom_printf("%s", str);
    }

    void print(int val) {
        esp_rom_printf("%d", val);
    }

    void print(unsigned long val) {
        esp_rom_printf("%lu", val);
    }

    void print(float val) {
        esp_rom_printf("%f", (double)val);
    }

    void println(const char* str = "") {
        esp_rom_printf("%s\n", str);
    }

    void println(int val) {
        esp_rom_printf("%d\n", val);
    }

    void println(unsigned long val) {
        esp_rom_printf("%lu\n", val);
    }

    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        esp_log_writev(ESP_LOG_INFO, LOG_TAG, format, args);
        va_end(args);
    }

    int available() { return 0; }
    int read() { return -1; }
};

extern SerialClass Serial;

#endif // __cplusplus

// ============================================================================
// MATH FUNCTIONS
// ============================================================================

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef constrain
#define constrain(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))
#endif

#ifndef map
static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#endif

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

typedef uint8_t byte;
typedef bool boolean;

// String - usar std::string em C++ ou char* em C
// Para compatibilidade básica com Arduino String, usar std::string

#ifdef __cplusplus
}
#endif

#endif // ARDUINO_COMPAT_H
