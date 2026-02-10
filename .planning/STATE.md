# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-09)

**Core value:** Rastrear estados de jornada de motoristas em tempo real com interface responsiva, comunicacao segura via BLE, e dados que nunca se perdem.
**Current focus:** Phase 2 - BLE Core (Phase 1 + 1.1 complete)

## Current Position

Phase: 2 of 5 (BLE Core)
Plan: 1 of 4 in current phase (02-01 NimBLE init complete)
Status: Executing Phase 02
Last activity: 2026-02-10 -- 02-01-PLAN.md complete (NimBLE BLE init + GAP advertising)

Progress: [██████░░░░] 40%

## Performance Metrics

**Velocity:**
- Total plans completed: 9
- Average duration: 4 min
- Total execution time: 0.68 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01-foundation | 4 | 15 min | 4 min |
| 01.1-screen-infra-hardening | 3 | 10 min | 3 min |
| 02-ble-core | 1 | 8 min | 8 min |

**Recent Trend:**
- Last 5 plans: 01.1-01 (2min), 01.1-02 (6min), 01.1-03 (2min), 01-04 (5min), 02-01 (8min)
- Trend: slight increase (BLE requires full rebuild + sdkconfig regeneration)

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
Stopped at: Completed 02-01-PLAN.md (NimBLE BLE init + GAP advertising). Ready for 02-02 (GATT services).
Resume file: .planning/phases/02-ble-core/02-02-PLAN.md
