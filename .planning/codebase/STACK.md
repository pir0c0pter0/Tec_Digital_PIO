# Technology Stack

**Analysis Date:** 2026-02-09

## Languages

**Primary:**
- C (80%) - Core HAL/driver code, board support package (`src/esp_bsp.c`, `src/esp_lcd_axs15231b.c`, `src/esp_lcd_touch.c`, `src/lv_port.c`)
- C++ (20%) - Higher-level services and UI managers (`src/main.cpp`, `src/simple_audio_manager.cpp`, `src/button_manager.cpp`, `src/jornada_keyboard.cpp`)

**Mixed compilation:** Project uses extern "C" blocks for C/C++ interoperability. Core drivers (LCD, touch, I2C) are C; services/UI layer is C++.

## Runtime

**Environment:**
- ESP32-S3 dual-core 240 MHz processor
- 16 MB QSPI flash (80 MHz)
- PSRAM enabled (external RAM)
- FreeRTOS 10.x (included with ESP-IDF)

**Architecture:**
- Core 0: UI rendering (LVGL), main event loop, system tasks
- Core 1: Audio decoding (minimp3 MP3 decoder) and I2S playback

**Build System:**
- PlatformIO Core (pio command)
- CMake (via ESP-IDF)
- espressif32 platform v6.9.0

## Frameworks

**Core:**
- ESP-IDF (Espressif IoT Development Framework) - Complete ESP32-S3 SDK via PlatformIO
  - Includes FreeRTOS kernel, drivers, HAL
  - Components used: driver, esp_timer, freertos, esp_log

**Graphics:**
- LVGL 8.4.0 (`lib/lvgl/`) - Lightweight embedded GUI library
  - 16-bit color depth (RGB565)
  - Buffer size: 480×320 pixels
  - Configured via `lib/lvgl/lv_conf.h` and `include/lv_conf.h`
  - Runs on Core 0 (thread-safe with mutex protection)

**Audio:**
- minimp3 (header-only library) - MP3 decoder
  - Located: `include/minimp3.h`
  - Configuration: `MINIMP3_NO_SIMD` (no SIMD acceleration)
  - Decodes MPEG Layer III MP3 at 44.1 kHz, 48 kHz (mono/stereo)

**Filesystem:**
- LittleFS (joltwallet__littlefs v1.14.8+)
  - 1 MB partition (`spiffs` in partitions.csv)
  - Mounted at `/littlefs`
  - Stores MP3 audio files + PNG splash image
  - Managed via CMake dependency: `joltwallet/littlefs: "^1.14.8"`

## Key Dependencies

**Critical - Framework & Platform:**
- espressif32@6.9.0 - PlatformIO platform for ESP32-S3
- FreeRTOS (included in ESP-IDF) - Real-time OS kernel, tasks, mutexes, queues
- ESP-IDF driver components - GPIO, I2C, I2S, SPI, QSPI, ADC

**Graphics & UI:**
- LVGL 8.4.0 (`lib/lvgl/`) - GUI rendering engine
- Custom LVGL filesystem driver (`src/lvgl_fs_driver.cpp`) - Bridges LVGL to LittleFS

**Audio:**
- minimp3 - MP3 decoding (embedded in `include/minimp3.h`)
- I2S driver (esp-idf/driver) - Audio output via I2S peripheral

**Hardware Access:**
- Custom AXS15231B driver (`src/esp_lcd_axs15231b.c`) - QSPI LCD controller for 3.5" display
- Custom touch driver (`src/esp_lcd_touch.c`) - Capacitive touch input (I2C protocol)
- GPIO driver - Ignition monitoring (GPIO18)
- I2C driver - Touch controller communication (SCL=8, SDA=4)
- I2S driver - Audio I2S output

**Local Libraries:**
- ESP32-audioI2S (`lib/ESP32-audioI2S/`) - Reference audio implementation (not directly used; minimp3 used instead)

## Configuration

**Environment:**
- Board: esp32-s3-devkitc-1 (generic ESP32-S3 board definition)
- Upload speed: 921600 baud (CDC/USB)
- Monitor speed: 115200 baud (serial)
- Flash frequency: 80 MHz, QIO mode

**Centralized Configuration:**
- `include/config/app_config.h` - 260+ lines defining ALL system constants
  - Display: 480×320 px, rotated 90°
  - Audio: I2S pins (BCK=42, LRCK=2, DOUT=41), volume 0-21
  - Touch: I2C pins (SCL=8, SDA=4), INT=3
  - Ignition: GPIO18 with dual debounce (1.0s ON, 2.0s OFF)
  - Colors, timings, button layout (4×3 grid), task priorities
  - Audio file paths: 11 MP3 files, 1 PNG splash

**Build Flags:**
```ini
-DLV_CONF_INCLUDE_SIMPLE        # LVGL config location
-DLV_LVGL_H_INCLUDE_SIMPLE      # LVGL header location
-DBOARD_HAS_PSRAM               # Enable PSRAM usage
```

**Compilation Warnings:**
- `-Wall -Wextra`
- `-Wno-unused-parameter`
- `-Wno-missing-field-initializers`

## Platform Requirements

**Development:**
- Host OS: Linux, macOS, or Windows
- PlatformIO CLI or IDE
- Python 3.6+ (for esptool/build tools)
- ESP-IDF tools (installed via PlatformIO)
- GCC ARM toolchain (espressif32 platform includes it)

**Build Verification:**
```bash
pio run                    # Build firmware
pio run --target upload    # Flash to ESP32-S3
pio run --target uploadfs  # Flash LittleFS (data/ → spiffs partition)
pio device monitor         # Serial console at 115200 baud
```

**Hardware Requirements:**
- ESP32-S3 devkit with USB CDC (appears as /dev/ttyACM0 on Linux)
- 3.5" JC3248W535EN display with AXS15231B controller (320×480 native, QSPI interface)
- Capacitive touch panel (I2C)
- Ignition signal input (GPIO18)
- I2S amplifier/speaker system (audio output)

**Firmware Size:**
- App partition (app0): 3 MB max (see partitions.csv)
- LittleFS (spiffs): 1 MB (stores 11 MP3s ~400KB + PNG splash ~12KB)

---

*Stack analysis: 2026-02-09*
