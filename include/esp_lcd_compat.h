/**
 * ESP-IDF LCD Compatibility Layer
 * Adiciona definições ausentes para compatibilidade com versões mais antigas
 */

#ifndef ESP_LCD_COMPAT_H
#define ESP_LCD_COMPAT_H

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// Compatibilidade para LCD_RGB_ELEMENT_ORDER
#ifndef LCD_RGB_ELEMENT_ORDER_RGB
#define LCD_RGB_ELEMENT_ORDER_RGB ESP_LCD_COLOR_SPACE_RGB
#endif

#ifndef LCD_RGB_ELEMENT_ORDER_BGR
#define LCD_RGB_ELEMENT_ORDER_BGR ESP_LCD_COLOR_SPACE_BGR
#endif

// Compatibilidade para quad_mode -> octal_mode
// Este é usado na configuração SPI, vamos redefinir a macro

#endif // ESP_LCD_COMPAT_H
