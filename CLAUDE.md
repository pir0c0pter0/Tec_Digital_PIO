# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 embedded firmware for a "Teclado de Jornada Digital" (Digital Driver Journey Keypad) — a 3.5" touchscreen device that tracks professional driver journey states (driving, maneuvering, meal breaks, waiting, unloading, refueling) with audio feedback and ignition monitoring. Built by Getscale Sistemas Embarcados.

## Build & Flash Commands

```bash
# Build firmware
pio run

# Flash firmware to ESP32-S3 (USB CDC on /dev/ttyACM0)
pio run --target upload

# Flash LittleFS filesystem (data/ folder → 1MB partition)
pio run --target uploadfs

# Serial monitor (115200 baud)
pio device monitor

# Full clean build
pio run --target clean && pio run
```

There is no test framework configured — this is bare-metal embedded code verified by flashing to hardware.

## Architecture

**Target:** ESP32-S3, dual-core 240MHz, 16MB flash, PSRAM, ESP-IDF 5.3.1 via PlatformIO (espressif32@6.9.0).

### Layered Design

```
main.cpp (orchestration & event loop)
    ↓
Services: IgnicaoService, JornadaService, AudioManager
    ↓
Abstract Interfaces: i_ignicao.h, i_jornada.h, i_audio.h, i_screen.h
    ↓
UI Components: ButtonManager (4×3 grid), Theme, StatusBar
    ↓
HAL/Drivers: AXS15231B LCD (QSPI), Touch (I2C), I2S Audio, GPIO
    ↓
ESP-IDF / FreeRTOS
```

### Core Distribution

- **Core 0:** UI rendering (LVGL), main loop, ignition polling, system_task
- **Core 1:** Audio decoding (minimp3) and I2S playback, dedicated audio_task

### Key Source Locations

| Area | Path | Purpose |
|------|------|---------|
| Entry point | `src/main.cpp` | app_main(), system_task loop (5ms tick) |
| Configuration | `include/config/app_config.h` | All 78+ system constants (pins, timings, colors, paths) |
| Interfaces | `include/interfaces/i_*.h` | Abstract service contracts |
| Ignition service | `src/services/ignicao/` | GPIO18 monitoring with dual debounce |
| Journey service | `src/services/jornada/` | Multi-driver state machine (up to 3 drivers, 7 states) |
| Audio | `src/simple_audio_manager.cpp` | minimp3 decoder → I2S, queued playback (4 slots) |
| UI buttons | `src/button_manager.cpp` | LVGL 4×3 button grid, popups, swipe navigation |
| LCD driver | `src/esp_lcd_axs15231b.c` | AXS15231B QSPI display controller |
| Touch driver | `src/esp_lcd_touch.c` | Capacitive I2C touch input |
| BSP | `src/esp_bsp.c` | Board support: display init, backlight, LVGL setup |
| Filesystem | `src/lvgl_fs_driver.cpp` | LittleFS ↔ LVGL bridge (drive 'A:') |
| Splash | `src/simple_splash.cpp` | PNG splash screen from LittleFS |

### Thread Safety

All shared data in services is protected by FreeRTOS mutexes. The callback pattern (IgnicaoCallback, JornadaCallback) decouples event producers from consumers.

### Display

- Hardware: 3.5" LCD, AXS15231B controller, 320×480 native, rotated 90° → 480×320 working resolution
- GUI: LVGL 8.4.0 (lib/lvgl/), configured via `include/lv_conf.h`
- Layout: 40px status bar (top) + 280px button grid area

### Flash Partitions (partitions.csv)

- `app0`: 3MB (factory app)
- `spiffs`: 1MB (LittleFS — audio MP3s + splash PNG)
- No OTA configured

### Audio Assets (data/)

11 MP3 files + 1 PNG splash image. Mounted as LittleFS at runtime. Audio paths defined as constants in `app_config.h`.

## Key Patterns to Follow

- **Centralized config:** All constants go in `include/config/app_config.h` — never hardcode pins, timings, or paths elsewhere
- **Interface-first:** New services should implement an abstract interface in `include/interfaces/`
- **Singleton services:** Services use static instance pattern with lazy init
- **C/C++ mixed:** Legacy modules are C-compatible (extern "C"), newer modules are C++
- **LVGL threading:** All LVGL calls must happen from Core 0 (system_task context)
- **Audio is async:** Use the audio queue, never decode in the UI task

## Hardware Pin Reference

Key GPIO assignments (full list in `app_config.h`):
- **QSPI LCD:** CS=45, SCK=47, D0-D3=21/48/40/39, Backlight=1
- **I2C Touch:** SCL=8, SDA=4, INT=3
- **I2S Audio:** BCK=42, LRCK=2, DOUT=41
- **Ignition:** GPIO18 (input, software debounce)

## Language

Code comments and UI strings are in **Portuguese (Brazilian)**. Commit messages use Portuguese. Variable/function names mix Portuguese domain terms (jornada, motorista, ignicao, manobra) with English programming conventions.
