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

### BLE Communication Protocol (BLE-P2.0)

#### Security Model

All communication uses **LE Secure Connections (Just Works)** with AES-128-CCM encryption.

- **Pairing:** Peripheral (ESP32) initiates via `ble_gap_security_initiate()` 500ms after connection (timer delay allows Android to complete service discovery first)
- **IO Capability:** `NO_INPUT_NO_OUTPUT` — no numeric comparison or passkey
- **Bonding:** Enabled — keys persisted in NVS (namespace `nimble_bond` on default `nvs` partition)
- **Key Distribution:** ENC + ID keys in both directions
- **Re-pairing:** `BLE_GAP_EVENT_REPEAT_PAIRING` handler deletes old bond and retries
- **MTU:** Preferred 512 bytes, negotiated after connection
- **Connection Parameters (post-encryption):** 30-50ms interval, 0 slave latency, 4s supervision timeout

#### Connection Lifecycle

```
1. App scans → finds "GS-Jornada-XXXX" (last 2 bytes of BT MAC)
2. App connects → ESP32: BLE_GAP_EVENT_CONNECT → status=CONNECTED
3. App discovers services → reads DIS (no encryption required)
4. [500ms timer] → ESP32 calls ble_gap_security_initiate()
5. Android receives pairing request → Just Works → auto-accept
6. LE Secure Connections established → AES-128-CCM active
7. ESP32: BLE_GAP_EVENT_ENC_CHANGE → status=SECURED
8. ESP32 requests connection parameters (30-50ms, 4s timeout)
9. App can now read/write encrypted custom characteristics
```

If the app attempts to read a custom characteristic before step 6, NimBLE returns `BLE_ATT_ERR_INSUFFICIENT_ENC` (0x0F).

#### BLE Status Enum

| Value | Name | Description |
|-------|------|-------------|
| 0 | DISCONNECTED | No connection, no advertising |
| 1 | ADVERTISING | Advertising active, waiting for connection |
| 2 | CONNECTED | Connected, encryption not yet established |
| 3 | SECURED | Connected with LE Secure Connections (encrypted) |

#### UUID Scheme

Custom UUIDs follow the pattern: `0000XXXX-4A47-0000-4763-7365-0000000Y`
- `XXXX` = service/characteristic short ID
- `Y` = service group (1=Journey, 2=Config, 3=Diagnostics, 4=OTA Prov)
- Note: `4A47` = "JG" (Jornada Getscale), `47637365` = "Gcse"

#### GATT Services Overview

| Service | UUID | Encryption | Characteristics |
|---------|------|------------|----------------|
| Device Info (SIG) | `0x180A` | None | Manufacturer, Model, FW Version, HW Revision, SW Revision (BLE-P2.0) |
| Journey | `00000100-...01` | Required | Journey State (R/N), Ignition Status (R/N) |
| Configuration | `00000200-...02` | Required | Volume (R/W/N), Brightness (R/W/N), Driver Name (R/W), Time Sync (W) |
| Diagnostics | `00000300-...03` | Required | System Diagnostics (R) |
| OTA Provisioning | `00000400-...04` | Required | Wi-Fi Credentials (W), OTA Status (R/N), IP Address (R/N) |

#### Device Information Service (DIS) — No Encryption

Standard SIG service `0x180A`. Readable **before** pairing for device identification.

| Characteristic | Value |
|---------------|-------|
| Manufacturer Name | `Getscale Sistemas Embarcados` |
| Model Number | `GS-Jornada` |
| Firmware Revision | `1.0.0` (APP_VERSION_STRING) |
| Hardware Revision | `ESP32-S3-R8` |
| Software Revision | `BLE-P2.0` (BLE_PROTOCOL_VERSION) |

#### Journey Service — `00000100-4A47-0000-4763-7365-00000001`

**Journey State** — UUID: `00000101-...-01` | Flags: READ_ENC, NOTIFY

Read returns 24 bytes (3 drivers x 8 bytes each), packed little-endian:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | motorist_id | Driver ID (1-3) |
| 1 | 1 | state | Journey state enum (0-6) |
| 2 | 1 | active | 1=active, 0=inactive |
| 3 | 1 | reserved | Alignment padding |
| 4 | 4 | time_in_state | Time in current state (ms), uint32 LE |

Repeats 3 times (driver 1 at offset 0, driver 2 at offset 8, driver 3 at offset 16).

Journey state enum values:
| Value | State | Portuguese |
|-------|-------|-----------|
| 0 | INACTIVE | Inativo |
| 1 | DRIVING | Jornada (direcao) |
| 2 | MANEUVERING | Manobra |
| 3 | MEAL_BREAK | Refeicao |
| 4 | WAITING | Espera |
| 5 | UNLOADING | Descarga |
| 6 | REFUELING | Abastecimento |

**Ignition Status** — UUID: `00000102-...-01` | Flags: READ_ENC, NOTIFY

Read returns 8 bytes, packed little-endian:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | status | 1=ignition ON, 0=OFF |
| 1 | 3 | reserved | Alignment padding |
| 4 | 4 | duration_ms | Duration in current state (ms), uint32 LE |

#### Configuration Service — `00000200-4A47-0000-4763-7365-00000002`

**Volume** — UUID: `00000201-...-02` | Flags: READ_ENC, WRITE_ENC, NOTIFY

- Read: 1 byte, range 0-21 (current volume from NVS)
- Write: 1 byte, range 0-21 (values >21 return `BLE_ATT_ERR_VALUE_NOT_ALLOWED`)
- Notify: 1 byte (sent when volume changes locally)

**Brightness** — UUID: `00000202-...-02` | Flags: READ_ENC, WRITE_ENC, NOTIFY

- Read: 1 byte, range 0-100 (current brightness percentage from NVS)
- Write: 1 byte, range 0-100 (values >100 return `BLE_ATT_ERR_VALUE_NOT_ALLOWED`)
- Notify: 1 byte (sent when brightness changes locally)

**Driver Name** — UUID: `00000203-...-02` | Flags: READ_ENC, WRITE_ENC

- Read: 99 bytes (3 drivers x 33 bytes each: 1 byte driver_id + 32 bytes name, null-padded)
- Write: 2-33 bytes (1 byte driver_id [1-3] + 1-32 bytes name UTF-8). Driver ID on BLE is 1-based (1-3), mapped internally to 0-based (0-2).

**Time Sync** — UUID: `00000204-...-02` | Flags: WRITE_ENC

- Write-only: 4 bytes little-endian uint32 Unix timestamp (seconds since epoch). Value 0 is rejected.

#### Diagnostics Service — `00000300-4A47-0000-4763-7365-00000003`

**System Diagnostics** — UUID: `00000301-...-03` | Flags: READ_ENC

Read returns 16 bytes, packed little-endian:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | free_heap | Internal heap free (bytes), uint32 LE |
| 4 | 4 | min_free_heap | Minimum free heap since boot (bytes), uint32 LE |
| 8 | 4 | psram_free | PSRAM free (bytes), uint32 LE |
| 12 | 4 | uptime_seconds | Uptime since boot (seconds), uint32 LE |

#### OTA Provisioning Service — `00000400-4A47-0000-4763-7365-00000004`

**Wi-Fi Credentials** — UUID: `00000401-...-04` | Flags: WRITE_ENC

Write format (3-98 bytes):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | ssid_len | SSID length (1-32) |
| 1 | ssid_len | ssid | SSID bytes (UTF-8) |
| 1+ssid_len | 1 | pwd_len | Password length (0-64) |
| 2+ssid_len | pwd_len | password | Password bytes (UTF-8) |

**OTA Status** — UUID: `00000402-...-04` | Flags: READ_ENC, NOTIFY

Read/Notify returns 2 bytes:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | state | OTA state machine value (0-11) |
| 1 | 1 | error_code | Error code (0=no error) |

OTA state values: 0=IDLE, 1=WIFI_CONNECTING, 2=WIFI_CONNECTED, 3=BLE_SHUTTING_DOWN, 4=HTTP_STARTING, 5=HTTP_READY, 6=RECEIVING, 7=VERIFYING, 8=WRITING, 9=REBOOTING, 10=ERROR, 11=ABORTED.

**IP Address** — UUID: `00000403-...-04` | Flags: READ_ENC, NOTIFY

Read/Notify returns 4 bytes: IPv4 address in network byte order (big-endian). Example: `192.168.1.100` = `0xC0A80164`.

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
- **BLE security:** All custom characteristics require encrypted link (_ENC flags). DIS stays unencrypted for pre-pairing discovery. Peripheral initiates pairing via 500ms timer after connection. Never add _ENC to DIS.

## Hardware Pin Reference

Key GPIO assignments (full list in `app_config.h`):
- **QSPI LCD:** CS=45, SCK=47, D0-D3=21/48/40/39, Backlight=1
- **I2C Touch:** SCL=8, SDA=4, INT=3
- **I2S Audio:** BCK=42, LRCK=2, DOUT=41
- **Ignition:** GPIO18 (input, software debounce)

## Language

Code comments and UI strings are in **Portuguese (Brazilian)**. Commit messages use Portuguese. Variable/function names mix Portuguese domain terms (jornada, motorista, ignicao, manobra) with English programming conventions.
