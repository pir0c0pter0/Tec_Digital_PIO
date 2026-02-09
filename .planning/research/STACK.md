# Technology Stack: BLE + Screen Manager + OTA Milestone

**Project:** Teclado de Jornada Digital v2.0
**Researched:** 2026-02-09
**Confidence:** MEDIUM (web search unavailable; based on training data knowledge of ESP-IDF 5.x, NimBLE, LVGL 8.4, verified against existing codebase)

## Recommended Stack

### BLE Communication

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| NimBLE (esp-nimble) | Bundled with ESP-IDF 5.3.1 | BLE 5.0 stack | 50% less RAM than Bluedroid (~50KB vs ~100KB), BLE-only (no Classic BT needed), built into ESP-IDF, full GATT server/client, LE Secure Connections support | HIGH |
| NimBLE GATT Server | ESP-IDF component | Custom BLE services | Standard GATT profile pattern, well-documented API, supports notifications/indications for real-time data push | HIGH |
| NimBLE Security Manager | ESP-IDF component | LE Secure Connections (LESC) | AES-CCM encryption, ECDH key exchange (P-256), supports Just Works and Numeric Comparison pairing models | HIGH |

### Screen Management

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| LVGL 8.4.0 (existing) | 8.4.0 | GUI framework | Already in project, `lv_scr_load_anim()` provides animated screen transitions, `lv_obj_create(NULL)` creates new screens | HIGH |
| Custom ScreenManager | N/A (build on i_screen.h) | Screen navigation framework | IScreen interface already defined in `include/interfaces/i_screen.h` with create/destroy/onEnter/onExit lifecycle; implement concrete ScreenManager using lv_scr_load_anim() | HIGH |
| LVGL Slider widget | 8.4.0 | Settings screen (volume, brightness) | Enable `LV_USE_SLIDER 1` in lv_conf.h (currently 0) | HIGH |
| LVGL Switch widget | 8.4.0 | Settings toggles | Enable `LV_USE_SWITCH 1` in lv_conf.h (currently 0) | HIGH |
| LVGL Dropdown widget | 8.4.0 | Selection lists | Enable `LV_USE_DROPDOWN 1` in lv_conf.h (currently 0) | MEDIUM |

### OTA Firmware Update

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| esp_ota_ops (ESP-IDF) | 5.3.1 | Low-level OTA API | `esp_ota_begin()`, `esp_ota_write()`, `esp_ota_end()`, `esp_ota_set_boot_partition()` -- core OTA primitives, handles partition switching and rollback | HIGH |
| esp_app_desc (ESP-IDF) | 5.3.1 | Firmware version tracking | `esp_app_get_description()` provides version info for OTA version comparison | HIGH |
| NVS (Non-Volatile Storage) | ESP-IDF 5.3.1 | Persist journey data, BLE bonds, settings | Already available in partition table (nvs partition at 0x9000), key-value storage survives OTA and reboots | HIGH |

### Data Persistence

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| NVS Flash (esp_nvs) | ESP-IDF 5.3.1 | Settings, BLE bonds, journey state | Built into ESP-IDF, already has partition in flash layout, supports typed key-value pairs, wear-leveled | HIGH |
| LittleFS (existing) | 1.14.8+ | Audio files, splash, OTA staging area | Already configured, can also store journey logs as files if NVS is insufficient for structured data | HIGH |

### Supporting Libraries

| Library | Version | Purpose | When to Use | Confidence |
|---------|---------|---------|-------------|------------|
| esp_timer | ESP-IDF 5.3.1 | High-resolution timers | BLE advertising intervals, OTA timeout watchdog | HIGH |
| esp_event | ESP-IDF 5.3.1 | Event loop for decoupled communication | Route BLE events to UI, decouple BLE service from screen manager | HIGH |
| cJSON | ESP-IDF bundled | JSON serialization | BLE protocol data packaging (if using JSON-based protocol), configuration export/import | MEDIUM |
| mbedtls | ESP-IDF bundled | Cryptographic primitives | SHA-256 for OTA firmware verification, additional BLE payload encryption if needed | HIGH |

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| BLE Stack | NimBLE | Bluedroid | Bluedroid uses ~100KB RAM (vs ~50KB NimBLE), supports Classic BT which is unnecessary, larger code size. NimBLE is the standard for BLE-only ESP32-S3 projects. |
| BLE Stack | NimBLE | Arduino BLE | Project uses ESP-IDF framework (not Arduino). Arduino BLE wraps Bluedroid anyway. |
| Screen Manager | Custom (on IScreen) | LVGL Fragment API | `lv_fragment` in LVGL 8.4 exists (`LV_USE_FRAGMENT`) but is experimental, poorly documented, and overkill for 4-6 screens. IScreen interface already exists in the project. |
| Screen Manager | Custom (on IScreen) | LVGL Tabview | Tabview loads all tabs simultaneously consuming RAM. Push/pop pattern is more memory-efficient for embedded -- only active screen exists in RAM. |
| OTA | BLE-based OTA (custom) | esp_https_ota | No Wi-Fi available on this device. esp_https_ota requires HTTP/HTTPS connection. Must implement OTA over BLE using esp_ota_ops primitives. |
| OTA | esp_ota_ops (low-level) | Third-party BLE DFU libs | No well-maintained ESP-IDF BLE DFU library exists. Nordic DFU protocol is tied to nRF chips. Custom GATT-based OTA with esp_ota_ops is the standard ESP32 approach. |
| Data Persistence | NVS | SQLite | Massive footprint for embedded. NVS handles key-value config perfectly. Journey logs can use LittleFS files if needed. |
| Protocol Format | Binary (TLV) | JSON over BLE | BLE MTU is typically 23-512 bytes. Binary TLV protocol is 2-3x more bandwidth efficient than JSON. Critical for OTA throughput over BLE. |

## New Partition Table (Required for OTA)

The current partition table does NOT support OTA:

```
# CURRENT (no OTA capability):
# app0:   3MB factory  (0x10000 - 0x30FFFF)
# spiffs: 1MB          (0x310000 - 0x40FFFF)
# Total used: ~4.1MB of 16MB
```

**Proposed OTA-capable partition table for 16MB flash:**

```csv
# Name,    Type, SubType,  Offset,    Size,      Flags
nvs,       data, nvs,      0x9000,    0x5000,
otadata,   data, ota,      0xe000,    0x2000,
phy_init,  data, phy,      0x10000,   0x1000,
ota_0,     app,  ota_0,    0x20000,   0x300000,
ota_1,     app,  ota_1,    0x320000,  0x300000,
spiffs,    data, spiffs,   0x620000,  0x100000,
nvs_keys,  data, nvs_keys, 0x720000,  0x1000,
```

| Partition | Size | Purpose |
|-----------|------|---------|
| nvs | 20KB | Settings, BLE bond info, journey state |
| otadata | 8KB | OTA boot selection metadata |
| phy_init | 4KB | PHY calibration (BLE RF) |
| ota_0 | 3MB | Active firmware slot A |
| ota_1 | 3MB | Firmware slot B (OTA target) |
| spiffs | 1MB | LittleFS (audio, splash, logs) |
| nvs_keys | 4KB | NVS encryption keys (optional, for secure NVS) |
| **Total** | ~7.15MB | Leaves ~8.8MB free of 16MB flash |

**Key changes from current layout:**
- `factory` partition replaced with `ota_0` + `ota_1` (dual-slot OTA)
- Added `otadata` partition (required by ESP-IDF OTA to track which slot is active)
- Added `nvs_keys` for future NVS encryption capability
- Both app slots remain 3MB each (enough for firmware + BLE + OTA code)

## sdkconfig.defaults Additions

```ini
# ============================================================================
# BLE / NimBLE
# ============================================================================
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
CONFIG_BT_NIMBLE_MAX_BONDS=3
CONFIG_BT_NIMBLE_ROLE_CENTRAL=n
CONFIG_BT_NIMBLE_ROLE_OBSERVER=n
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_BT_NIMBLE_ROLE_BROADCASTER=y
CONFIG_BT_NIMBLE_PINNED_TO_CORE_0=y
CONFIG_BT_NIMBLE_TASK_STACK_SIZE=6144

# Security: LE Secure Connections
CONFIG_BT_NIMBLE_SM_LEGACY=n
CONFIG_BT_NIMBLE_SM_SC=y
CONFIG_BT_NIMBLE_SMP_LE_SC_ENABLED=y

# BLE 5.0 features
CONFIG_BT_NIMBLE_EXT_ADV=n
CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT=n

# Memory optimization for BLE
CONFIG_BT_NIMBLE_LOG_LEVEL_WARNING=y
CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT=12
CONFIG_BT_NIMBLE_MSYS_1_BLOCK_SIZE=256
CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT=24
CONFIG_BT_NIMBLE_MSYS_2_BLOCK_SIZE=320
CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=247

# ============================================================================
# OTA
# ============================================================================
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_APP_ROLLBACK_ENABLE=y

# ============================================================================
# NVS
# ============================================================================
CONFIG_NVS_ENCRYPTION=n
```

## lv_conf.h Changes Required

```c
// Enable widgets needed for settings screens:
#define LV_USE_SLIDER     1   // Volume and brightness controls
#define LV_USE_SWITCH     1   // Toggle settings (BLE on/off, etc.)
#define LV_USE_DROPDOWN   1   // Selection lists
#define LV_USE_BAR        1   // Progress bars (OTA progress) - already enabled
#define LV_USE_ARC        1   // Circular progress (optional, for OTA)

// Enable for screen transitions:
// lv_scr_load_anim() already works with default config
// No additional lv_conf.h changes needed for screen transitions
```

## GATT Service Design (BLE Protocol)

```
Service: Journey Keypad (custom UUID)
|
+-- Characteristic: Device Info (read)
|   UUID: custom
|   Properties: Read
|   Value: {fw_version, hw_version, device_name, serial}
|
+-- Characteristic: Journey State (read/notify)
|   UUID: custom
|   Properties: Read, Notify
|   Value: {driver_id, state, timestamp}
|
+-- Characteristic: Ignition Status (read/notify)
|   UUID: custom
|   Properties: Read, Notify
|   Value: {status, duration_ms}
|
+-- Characteristic: Configuration (read/write)
|   UUID: custom
|   Properties: Read, Write
|   Value: {volume, brightness, debounce_on, debounce_off, ...}
|
+-- Characteristic: Command (write)
|   UUID: custom
|   Properties: Write
|   Value: {command_id, payload}
|
+-- Characteristic: Journey Log (read/notify)
|   UUID: custom
|   Properties: Read, Notify
|   Value: {log_entries[], pagination}

Service: OTA Update (custom UUID)
|
+-- Characteristic: OTA Control (write/notify)
|   UUID: custom
|   Properties: Write, Notify
|   Value: {command: START/ABORT/CONFIRM, fw_size, fw_hash}
|
+-- Characteristic: OTA Data (write-no-response)
|   UUID: custom
|   Properties: Write Without Response
|   Value: {chunk_data[]} (max MTU - 3 bytes per write)
|
+-- Characteristic: OTA Status (read/notify)
|   UUID: custom
|   Properties: Read, Notify
|   Value: {state, progress_pct, error_code, current_version}
```

## Memory Budget Estimate

| Component | RAM (approx) | Flash (approx) | Notes |
|-----------|-------------|-----------------|-------|
| NimBLE stack | ~50KB | ~180KB | With security manager, 1 connection |
| NimBLE task stack | 6KB | - | Configurable via CONFIG_BT_NIMBLE_TASK_STACK_SIZE |
| BLE service data | ~2KB | ~5KB | GATT table, attribute values |
| Screen Manager | ~1KB | ~8KB | Navigation stack, screen registry |
| Settings screen | ~4KB | ~12KB | LVGL widgets (sliders, switches) |
| OTA buffer | ~4KB | ~10KB | Write buffer for esp_ota_write |
| NVS operations | ~3KB | Partition | In-flash storage, minimal RAM |
| **Total new** | **~70KB** | **~215KB** | Well within PSRAM/flash budget |

**Current firmware size (estimated):** ~1.5-2MB (3MB partition is half-full)
**With BLE+OTA additions:** ~2.0-2.5MB (still fits in 3MB OTA slots)

## Core Affinity Plan

| Task | Core | Priority | Stack Size | Rationale |
|------|------|----------|------------|-----------|
| system_task (LVGL) | 0 | 5 | 8KB | Existing -- UI rendering, touch input |
| audio_task | 1 | 5 | 32KB | Existing -- MP3 decode, I2S playback |
| NimBLE host task | 0 | 4 | 6KB | BLE stack runs on Core 0, lower priority than UI to avoid display stutter |
| BLE service task | 0 | 3 | 4KB | Handles GATT callbacks, lower than NimBLE host |

**Why BLE on Core 0 (not Core 1):** Core 1 is dedicated to real-time audio decoding. BLE events are bursty but not latency-critical. NimBLE on Core 0 shares with UI but at lower priority, so LVGL always wins for responsiveness. The ESP-IDF NimBLE component defaults to Core 0 pinning.

## Installation / Build Changes

```ini
# platformio.ini additions:

# No lib_deps needed for NimBLE -- it's an ESP-IDF component
# No lib_deps needed for esp_ota_ops -- it's an ESP-IDF component
# No lib_deps needed for NVS -- it's an ESP-IDF component

# Add to sdkconfig.defaults (see section above)

# Update partition table:
# board_build.partitions = partitions_ota.csv

# Add build flags for BLE:
build_flags =
    -DLV_CONF_INCLUDE_SIMPLE
    -DLV_LVGL_H_INCLUDE_SIMPLE
    -DBOARD_HAS_PSRAM
    -DCONFIG_BT_NIMBLE_ENABLED
```

## What NOT to Use

| Technology | Why Avoid |
|------------|-----------|
| Bluedroid BLE stack | 2x RAM of NimBLE, includes unused Classic BT, slower startup |
| Arduino BLE library | Project is ESP-IDF, not Arduino framework. Would require framework change. |
| LVGL 9.x | Major breaking API change from 8.x. Migration would touch every UI file. Not worth it mid-project. |
| lv_fragment (LVGL 8.4) | Experimental API, poorly documented, designed for desktop-like fragment lifecycles. Overkill for 4-6 embedded screens. |
| esp_https_ota | Requires HTTP/HTTPS connection. No Wi-Fi on this device. |
| Nordic DFU protocol | Proprietary to Nordic Semiconductor nRF chips. Not compatible with ESP32 OTA partition scheme. |
| Protobuf over BLE | Heavy library footprint (~50-100KB flash). TLV binary protocol achieves same goal with zero dependencies. |
| Wi-Fi for OTA | Device spec excludes Wi-Fi. BLE OTA is sufficient for firmware sizes under 3MB. |
| SQLite for data | ~500KB flash footprint. NVS + LittleFS files cover all persistence needs. |
| FlatBuffers | Overkill for BLE MTU-sized packets. Simple TLV/struct serialization is faster and smaller. |

## Sources

- Training data knowledge of ESP-IDF 5.x NimBLE integration (MEDIUM confidence -- web verification unavailable)
- Training data knowledge of LVGL 8.4 screen management API (MEDIUM confidence)
- Training data knowledge of ESP-IDF OTA partition requirements (MEDIUM confidence)
- Existing codebase analysis: `platformio.ini`, `sdkconfig.defaults`, `partitions.csv`, `include/config/app_config.h`, `include/lv_conf.h`, `include/interfaces/i_screen.h` (HIGH confidence)
- ESP-IDF documentation patterns for NimBLE (training data, MEDIUM confidence)

**IMPORTANT NOTE:** Web search and web fetch were unavailable during this research session. All ESP-IDF API details, NimBLE configuration options, and OTA partition requirements are based on training data knowledge (cutoff: May 2025). The sdkconfig option names, partition table format, and LVGL API calls should be verified against the actual ESP-IDF 5.3.1 documentation before implementation. The overall architecture and technology choices are well-established patterns with HIGH confidence, but specific config key names may need adjustment.
