---
phase: 04-ota
plan: 03
subsystem: ota, ui, main
tags: [esp-ota, self-test, rollback, watchdog, navigation-lock, lvgl, main-integration]

# Dependency graph
requires:
  - phase: 04-ota-01
    provides: "OTA GATT provisioning service, BLE shutdown, OTA types and constants"
  - phase: 04-ota-02
    provides: "OtaService state machine, Wi-Fi STA, HTTP server, OtaScreen, progress queue"
  - phase: 02-ble-core
    provides: "BleService singleton with init() for self-test validation"
provides:
  - "Post-OTA self-test with automatic rollback on failure"
  - "Task watchdog (60s) protecting against hung self-test"
  - "Navigation lock in IScreenManager preventing screen changes during OTA"
  - "Full OTA integration in main.cpp: self-test at boot, OtaScreen, state machine, provisioning events"
  - "OTA failure handling with 3s error display then reboot"
affects: [05-polish, main-integration, ota-flow]

# Tech tracking
tech-stack:
  added: [esp_task_wdt, esp_ota_ops-self-test]
  patterns: [self-test-at-boot, navigation-lock-interface, ota-event-driven-main-loop]

key-files:
  created:
    - include/services/ota/ota_self_test.h
    - src/services/ota/ota_self_test.cpp
  modified:
    - include/interfaces/i_screen.h
    - include/ui/screen_manager.h
    - src/ui/screen_manager.cpp
    - src/ui/widgets/status_bar.cpp
    - src/main.cpp

key-decisions:
  - "esp_task_wdt_reconfigure (not esp_task_wdt_init) for watchdog since WDT already initialized by ESP-IDF -- reconfigure timeout to 60s during self-test"
  - "Self-test runs before normal init sequence but after LVGL (display init in app_main before system_task) -- NVS/BLE/audio inited inside self-test are idempotent"
  - "Navigation lock added to IScreenManager interface (pure virtual) not just ScreenManagerImpl -- enables StatusBar to check lock via IScreenManager pointer"
  - "OtaScreen not in normal screen cycle (NUMPAD->JORNADA->SETTINGS) -- accessed only via BLE provisioning trigger"
  - "OTA progress callback set via OtaService::setProgressCallback instead of lambda captures -- compatible with C function pointer pattern"

patterns-established:
  - "Self-test pattern: check partition state, run subsystem diagnostics, mark valid or rollback"
  - "Navigation lock pattern: interface method on IScreenManager, guards on navigateTo/goBack/cycleTo/swap button"
  - "OTA main loop integration: prov events -> state machine process -> progress callback -> failure reboot"

# Metrics
duration: 4min
completed: 2026-02-10
---

# Phase 4 Plan 3: OTA Self-Test and Main Integration Summary

**Post-OTA self-test with watchdog-protected rollback, navigation lock during OTA, and full main.cpp integration wiring provisioning events through state machine to OtaScreen**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-10T19:57:32Z
- **Completed:** 2026-02-10T20:01:42Z
- **Tasks:** 1 of 2 (Task 2 is hardware verification checkpoint)
- **Files modified:** 7

## Accomplishments
- Self-test module validates LVGL display, NVS, BLE, and audio on first boot after OTA
- Task watchdog (60s) protects against hung self-test -- if self-test hangs, device reboots and bootloader rolls back
- Navigation lock prevents screen switching during OTA via pure virtual IScreenManager methods
- StatusBar swap button respects navigation lock
- OtaScreen registered, pre-created, and wired to receive progress events from OTA state machine
- Full OTA event pipeline in main.cpp: BLE provisioning -> OtaScreen -> state machine -> progress -> reboot

## Task Commits

Each task was committed atomically:

1. **Task 1: Self-test module + navigation lock + main.cpp OTA integration** - `2698de7` (feat)

Task 2 is a hardware verification checkpoint (not executed by automation).

## Files Created/Modified
- `include/services/ota/ota_self_test.h` - Self-test function declaration (C linkage)
- `src/services/ota/ota_self_test.cpp` - Self-test: partition state check, LVGL/NVS/BLE/audio diagnostics, mark valid/rollback, watchdog
- `include/interfaces/i_screen.h` - Added isNavigationLocked() and setNavigationLocked() to IScreenManager
- `include/ui/screen_manager.h` - Added navigation lock methods and member to ScreenManagerImpl
- `src/ui/screen_manager.cpp` - Navigation lock guards on navigateTo/goBack/cycleTo, lock/unlock implementation
- `src/ui/widgets/status_bar.cpp` - Swap button callback checks navigation lock before cycling
- `src/main.cpp` - Self-test at boot, OtaScreen registration, OTA prov event queue, state machine in loop, progress callback, failure reboot

## Decisions Made
- Used `esp_task_wdt_reconfigure()` instead of `esp_task_wdt_init()` because ESP-IDF already initializes the WDT -- reconfigure sets 60s timeout during self-test, then restores 5s default
- Self-test checks run in order: LVGL (already init by app_main) -> NVS (idempotent init) -> BLE (idempotent init) -> Audio (idempotent init)
- Navigation lock added to IScreenManager interface rather than just implementation, allowing StatusBar to check via its IScreenManager* pointer without casting
- OtaScreen is not part of the normal screen cycle -- it's only activated by BLE OTA provisioning events, menu button skips it
- OTA progress uses `setProgressCallback` with static function pointer (not lambda) for C compatibility

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Checkpoint Pending

Task 2 requires hardware verification of the end-to-end OTA flow. The user must:
1. Flash firmware via USB
2. Connect via BLE (nRF Connect)
3. Write Wi-Fi credentials to OTA Provisioning characteristic
4. Upload firmware via HTTP to device IP
5. Verify self-test passes and firmware is confirmed

## Next Phase Readiness
- Complete OTA system implemented (pending hardware verification)
- Phase 05 (Polish) can proceed once OTA is verified on hardware
- All 3 plans of Phase 04 code-complete

## Self-Check: PASSED

All 7 files verified present. Task 1 commit (2698de7) verified in git log. SUMMARY.md exists. Build passes with zero errors (RAM: 14.5%, Flash: 49.4%).

---
*Phase: 04-ota*
*Completed: 2026-02-10 (pending hardware verification)*
