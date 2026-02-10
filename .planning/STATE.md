# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-09)

**Core value:** Rastrear estados de jornada de motoristas em tempo real com interface responsiva, comunicacao segura via BLE, e dados que nunca se perdem.
**Current focus:** Phase 1.1 - Screen Infrastructure Hardening

## Current Position

Phase: 1.1 of 5 (Screen Infrastructure Hardening)
Plan: 1 of 3 in current phase (01.1-01 complete)
Status: In Progress
Last activity: 2026-02-10 -- 01.1-01-PLAN.md complete (2 tasks, 2 commits)

Progress: [████░░░░░░] 17%

## Performance Metrics

**Velocity:**
- Total plans completed: 4 (01-03 pending hardware verify)
- Average duration: 3 min
- Total execution time: 0.20 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01-foundation | 3 | 10 min | 3 min |
| 01.1-screen-infra-hardening | 1 | 2 min | 2 min |

**Recent Trend:**
- Last 5 plans: 01-01 (2min), 01-02 (5min), 01-03 (3min), 01.1-01 (2min)
- Trend: stable

| Phase | Plan | Duration | Tasks | Files |
|-------|------|----------|-------|-------|
| 01 | P01 | 2min | 2 tasks | 4 files |
| 01 | P02 | 5min | 2 tasks | 5 files |
| 01 | P03 | 3min | 2 tasks | 5 files |
| 01.1 | P01 | 2min | 2 tasks | 2 files |

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

### Pending Todos

None yet.

### Roadmap Evolution

- Phase 1.1 inserted after Phase 1: Screen Infrastructure Hardening (URGENT) — Fix singleton fallback in event handlers, eliminate domain logic singletons (JornadaKeyboard/NumpadExample), fix static debounce sharing, fix destructor memory leak, prepare architecture for configurable multi-screen system

### Blockers/Concerns

- PSRAM migration of LVGL display buffers must happen before NimBLE is enabled (internal SRAM starvation risk)
- Partition table change is irreversible -- requires USB reflash of all existing devices
- Actual NimBLE memory footprint (~50KB estimate) must be verified empirically on hardware

## Session Continuity

Last session: 2026-02-10
Stopped at: Completed 01.1-01-PLAN.md (ButtonManager hardening). Ready for 01.1-02-PLAN.md.
Resume file: .planning/phases/01.1-screen-infrastructure-hardening/01.1-02-PLAN.md
