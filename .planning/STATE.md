# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-09)

**Core value:** Rastrear estados de jornada de motoristas em tempo real com interface responsiva, comunicacao segura via BLE, e dados que nunca se perdem.
**Current focus:** Phase 3 - Settings + Config Sync (Phase 1 + 1.1 + 2 complete)

## Current Position

Phase: 3 of 5 (Settings + Config Sync)
Plan: 1 of 3 in current phase (03-01 SettingsScreen + NVS driver names complete)
Status: Executing Phase 03
Last activity: 2026-02-10 -- 03-01-PLAN.md complete (SettingsScreen UI + NVS driver name persistence)

Progress: [████████████░░░░░░░░] 60%

## Performance Metrics

**Velocity:**
- Total plans completed: 13
- Average duration: 5 min
- Total execution time: 1.12 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01-foundation | 4 | 15 min | 4 min |
| 01.1-screen-infra-hardening | 3 | 10 min | 3 min |
| 02-ble-core | 4 | 27 min | 7 min |
| 03-settings | 1 | 7 min | 7 min |

**Recent Trend:**
- Last 5 plans: 02-01 (8min), 02-02 (7min), 02-03 (8min), 02-04 (4min), 03-01 (7min)
- Trend: Consistent 7min for UI+NVS work with pre-existing build fix

| Phase | Plan | Duration | Tasks | Files |
|-------|------|----------|-------|-------|
| 01 | P01 | 2min | 2 tasks | 4 files |
| 01 | P02 | 5min | 2 tasks | 5 files |
| 01 | P03 | 3min | 2 tasks | 5 files |
| 01 | P04 | 5min | 2 tasks | 8 files |
| 01.1 | P01 | 2min | 2 tasks | 2 files |
| 01.1 | P02 | 6min | 2 tasks | 9 files |
| 01.1 | P03 | 2min | 2 tasks | 2 files |
| 02 | P01 | 8min | 2 tasks | 7 files |
| 02 | P02 | 7min | 2 tasks | 7 files |
| 02 | P03 | 8min | 2 tasks | 4 files |
| 02 | P04 | 4min | 2 tasks | 5 files |
| 03 | P01 | 7min | 2 tasks | 8 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- NimBLE stack (not Bluedroid): smaller footprint, BLE-only, included in ESP-IDF 5.3.1
- Screen Manager with stack pattern: push/pop navigation, extends existing IScreen/IScreenManager interfaces
- Menu button in StatusBar (not swipe): avoids conflict with existing numpad/jornada swipe
- GATT-based BLE protocol: standard BLE, compatible with iOS CoreBluetooth and Android BLE API
- OTA via BLE (not Wi-Fi): device has no Wi-Fi configured, BLE already available
- Dual OTA partitions: allows rollback if OTA fails, critical for field devices
- NVS partition reduced from 0x5000 to 0x4000 to accommodate otadata, still above 12KB minimum
- Dedicated nvs_data partition (64KB) for app settings, separate from system NVS
- Flash layout uses 7.125MB of 16MB (44.5%), leaving 8.875MB unallocated
- Fixed-size navigation stack (no heap) for embedded reliability
- auto_del=false in lv_scr_load_anim() for manual screen lifecycle management
- StatusBar on lv_layer_top() with IScreenManager* for loose coupling
- [Phase 01]: Wrap existing JornadaKeyboard/NumpadExample rather than rewrite for v1.x behavior preservation
- [Phase 01]: Static allocation for screens and StatusBar in main.cpp (no heap for embedded reliability)
- [Phase 01.1]: Replace singleton fallback with ESP_LOGE + return to enforce strict screen isolation
- [Phase 01.1]: Use temporary blank screen swap when destroying active screen to avoid LVGL undefined state
- [Phase 01.1]: Lambda capture of self pointer for std::function callbacks; lv_obj user_data for LVGL C callbacks
- [Phase 01.1]: Delete domain logic object before ButtonManager in screen destructor (dependency order)
- [Phase 01-04]: NvsKbMotoristaState bitmap (3 bytes) for keyboard state instead of NvsJornadaState -- matches per-action-per-motorist model
- [Phase 01-04]: StateChangeCallback on JornadaKeyboard for decoupled NVS auto-save on every processarAcao
- [Phase 01-04]: NVS access always via NvsManager singleton -- never direct nvs_* calls elsewhere
- [Phase 02-01]: ble_store_config_init() does not exist in ESP-IDF 5.3.1 NimBLE -- bond store auto-configures via CONFIG_BT_NIMBLE_NVS_PERSIST=y
- [Phase 02-01]: Service UUIDs in scan response (not adv data) to stay under 31-byte limit
- [Phase 02-01]: BLE init after systemInitialized=true to avoid blocking boot sequence
- [Phase 02-01]: Must delete sdkconfig.esp32s3_idf when changing sdkconfig.defaults -- PlatformIO caches aggressively
- [Phase 02-02]: Built-in ble_svc_dis for DIS (0x180A) -- no custom implementation, ESP-IDF 5.3.1 includes it
- [Phase 02-02]: extern "C" wrapper for ble_svc_dis.h -- lacks C++ linkage guards unlike other NimBLE headers
- [Phase 02-02]: C++ designated initializer field order must match struct declaration order (ble_gatt_chr_def)
- [Phase 02-03]: LV_SYMBOL_BLUETOOTH available in LVGL 8.4.0 Montserrat 14 font (no fallback needed)
- [Phase 02-03]: BLE icon at x=130 between ignition timer and center area
- [Phase 02-03]: Event queue 8 items with zero-timeout on both sides (non-blocking)
- [Phase 02-04]: Per-characteristic subscription tracking via gatt_journey_update_subscription(attr_handle) -- prevents subscriptions from resetting each other
- [Phase 02-04]: Direct ble_gatts_notify_custom() calls from domain callbacks (Core 0) -- NimBLE queues internally
- [Phase 02-04]: gatt_validation.h header-only with NimBLE includes for self-contained Phase 3 usage
- [Phase 03-01]: Direct LVGL widgets on screen_ (no ButtonManager) for SettingsScreen -- sliders not button grid
- [Phase 03-01]: display.h include for bsp_display_brightness_set (not esp_bsp.h) -- function declared in display.h
- [Phase 03-01]: nvs_set_str for driver names (not blob) -- handles null-termination correctly
- [Phase 03-01]: 1000ms info refresh throttle -- system info labels dont need 5ms update rate

### Pending Todos

None yet.

### Roadmap Evolution

- Phase 1.1 inserted after Phase 1: Screen Infrastructure Hardening (URGENT) -- completed 2026-02-10

### Blockers/Concerns

- PSRAM migration of LVGL display buffers must happen before NimBLE is enabled (internal SRAM starvation risk)
- Partition table change is irreversible -- requires USB reflash of all existing devices
- Actual NimBLE memory footprint (~50KB estimate) must be verified empirically on hardware

## Session Continuity

Last session: 2026-02-10
Stopped at: Completed 03-01-PLAN.md (SettingsScreen + NVS driver names). Phase 03 in progress (1 of 3 plans done).
Resume file: .planning/phases/03-settings/03-02-PLAN.md
