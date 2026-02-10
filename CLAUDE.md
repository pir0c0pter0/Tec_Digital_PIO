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
Services: IgnicaoService, JornadaService, AudioManager, BleService, NvsManager, OtaService
    ↓
BLE: NimBLE GATT Server (Journey, Config, Diagnostics, OTA Provisioning, DIS services)
    ↓
OTA: Wi-Fi STA + HTTP Server (port 8080) + SHA-256 + Self-Test + Rollback
    ↓
Abstract Interfaces: i_ignicao.h, i_jornada.h, i_audio.h, i_screen.h, i_ble.h, i_nvs.h, i_ota.h
    ↓
Screen Manager: ScreenManagerImpl (push/pop stack, 4 screens, navigation lock)
    ↓
UI Screens: JornadaScreen, NumpadScreen, SettingsScreen, OtaScreen + StatusBar (lv_layer_top)
    ↓
UI Components: ButtonManager (4×3 grid), Theme, LVGL Sliders
    ↓
HAL/Drivers: AXS15231B LCD (QSPI), Touch (I2C), I2S Audio, GPIO
    ↓
ESP-IDF 5.3.1 / FreeRTOS / NimBLE
```

### Core Distribution

- **Core 0:** UI rendering (LVGL), main loop, ignition polling, system_task
- **Core 1:** Audio decoding (minimp3) and I2S playback, dedicated audio_task

### Key Source Locations

| Area | Path | Purpose |
|------|------|---------|
| Entry point | `src/main.cpp` | app_main(), system_task loop (5ms tick), screen/BLE/config/OTA event processing |
| Configuration | `include/config/app_config.h` | All system constants (pins, timings, colors, paths, NVS keys, OTA params) |
| BLE UUIDs | `include/config/ble_uuids.h` | All 128-bit UUIDs (Journey, Config, Diagnostics, OTA Prov groups) |
| Interfaces | `include/interfaces/i_*.h` | Abstract contracts: screen, BLE, NVS, ignicao, jornada, audio, OTA |
| Screen manager | `src/ui/screens/screen_manager.cpp` | Push/pop navigation stack, cycleTo(), animated transitions |
| Jornada screen | `src/ui/screens/jornada_screen.cpp` | Journey keypad screen (wraps ButtonManager) |
| Numpad screen | `src/ui/screens/numpad_screen.cpp` | Numeric keypad screen (wraps ButtonManager) |
| Settings screen | `src/ui/screens/settings_screen.cpp` | Volume/brightness sliders, system info (direct LVGL widgets) |
| Status bar | `src/ui/widgets/status_bar.cpp` | Persistent bar on lv_layer_top() with ignition/BLE/menu |
| BLE service | `src/services/ble/ble_service.cpp` | NimBLE singleton, GAP handler, connection management |
| GATT server | `src/services/ble/gatt/gatt_server.cpp` | Central GATT table (DIS + Journey + Diagnostics + Config + OTA Prov) |
| GATT journey | `src/services/ble/gatt/gatt_journey.cpp` | Journey state + ignition characteristics (read/notify) |
| GATT config | `src/services/ble/gatt/gatt_config.cpp` | Volume/brightness/driver name/time sync (read/write/notify) |
| GATT diagnostics | `src/services/ble/gatt/gatt_diagnostics.cpp` | Heap, uptime, queue depth (read-only) |
| GATT validation | `include/services/ble/gatt/gatt_validation.h` | Header-only write validation utility |
| GATT OTA prov | `src/services/ble/gatt/gatt_ota_prov.cpp` | Wi-Fi creds write, OTA status/IP notify, prov event queue |
| BLE event queue | `src/services/ble/ble_event_queue.cpp` | FreeRTOS queue for BLE status → UI |
| OTA service | `src/services/ota/ota_service.cpp` | 12-state OTA state machine (IOtaService impl) |
| OTA Wi-Fi | `src/services/ota/ota_wifi.cpp` | Wi-Fi STA non-blocking connect/poll/shutdown |
| OTA HTTP server | `src/services/ota/ota_http_server.cpp` | HTTP POST /update, 4KB streaming, SHA-256 verify |
| OTA self-test | `src/services/ota/ota_self_test.cpp` | Post-OTA boot validation + watchdog + rollback |
| OTA types | `include/services/ota/ota_types.h` | OtaState enum (12), progress/credentials structs |
| OTA screen | `src/ui/screens/ota_screen.cpp` | Progress bar, percentage, state text (no interaction) |
| NVS manager | `src/services/nvs/nvs_manager.cpp` | Persistent storage (settings, journey state, driver names) |
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

All shared data in services is protected by FreeRTOS mutexes. The callback pattern (IgnicaoCallback, JornadaCallback) decouples event producers from consumers. BLE-to-UI communication uses three FreeRTOS event queues (BLE status events + config events + OTA provisioning events) to safely bridge NimBLE task to Core 0 where LVGL and NVS operations happen. OTA progress uses a separate queue (size 1, xQueueOverwrite) from HTTP server task to system_task. NvsManager mutex protects flash access from any task context. Navigation lock in ScreenManager prevents screen switching during OTA.

### Display

- Hardware: 3.5" LCD, AXS15231B controller, 320×480 native, rotated 90° → 480×320 working resolution
- GUI: LVGL 8.4.0 (lib/lvgl/), configured via `include/lv_conf.h`
- Layout: 40px status bar (top) + 280px button grid area

### Flash Partitions (partitions.csv)

- `ota_0`: 3MB (primary app)
- `ota_1`: 3MB (OTA update target)
- `otadata`: 8KB (OTA boot selector)
- `nvs`: 16KB (system NVS)
- `nvs_data`: 64KB (app settings — volume, brightness, driver names, journey state)
- `spiffs`: 1MB (LittleFS — audio MP3s + splash PNG)

### BLE GATT Services

| Service | UUID | Characteristics |
|---------|------|----------------|
| Device Info (SIG) | 0x180A | Manufacturer, Model, FW Version, HW Revision |
| Journey (custom) | group 1 | State per driver (R/N), Time-in-state (R/N), Ignition (R/N) |
| Configuration (custom) | group 2 | Volume (R/W/N), Brightness (R/W/N), Driver Name (R/W), Time Sync (W) |
| Diagnostics (custom) | group 3 | Heap, Uptime, Queue Depth (R) |
| OTA Provisioning (custom) | group 4 | Wi-Fi Creds (W), OTA Status (R/N), IP Address (R/N) |

### Audio Assets (data/)

11 MP3 files + 1 PNG splash image. Mounted as LittleFS at runtime. Audio paths defined as constants in `app_config.h`.

## Key Patterns to Follow

- **Centralized config:** All constants go in `include/config/app_config.h` — never hardcode pins, timings, or paths elsewhere
- **Interface-first:** New services should implement an abstract interface in `include/interfaces/`
- **Singleton services:** Services use static instance pattern with lazy init
- **C/C++ mixed:** Legacy modules are C-compatible (extern "C"), newer modules are C++
- **LVGL threading:** All LVGL calls must happen from Core 0 (system_task context)
- **Audio is async:** Use the audio queue, never decode in the UI task
- **BLE-to-UI via queues:** Never call LVGL or blocking NVS from NimBLE callbacks — post events to FreeRTOS queues
- **GATT module pattern:** Each service in separate gatt_*.h/.cpp with C linkage, packed structs, per-characteristic subscription tracking
- **Screen lifecycle:** Screens implement IScreen (create/destroy/onEnter/onExit/update), managed by ScreenManager stack
- **NVS via NvsManager:** Never call nvs_* directly — always go through NvsManager singleton
- **Designated initializer order:** C++ struct init fields MUST match declaration order (especially ble_gatt_chr_def)
- **OTA flow:** BLE provisioning → Wi-Fi STA connect → BLE shutdown → HTTP server → firmware stream → SHA-256 verify → reboot → self-test → mark valid/rollback
- **Navigation lock:** Screen switching disabled during OTA via IScreenManager::setNavigationLocked(). OtaScreen not in normal screen cycle.
- **Self-test at boot:** After OTA reboot, ota_self_test() validates subsystems before marking firmware valid. 60s watchdog protects against hang.

## Hardware Pin Reference

Key GPIO assignments (full list in `app_config.h`):
- **QSPI LCD:** CS=45, SCK=47, D0-D3=21/48/40/39, Backlight=1
- **I2C Touch:** SCL=8, SDA=4, INT=3
- **I2S Audio:** BCK=42, LRCK=2, DOUT=41
- **Ignition:** GPIO18 (input, software debounce)

## Language

Code comments and UI strings are in **Portuguese (Brazilian)**. Commit messages use Portuguese. Variable/function names mix Portuguese domain terms (jornada, motorista, ignicao, manobra) with English programming conventions.
