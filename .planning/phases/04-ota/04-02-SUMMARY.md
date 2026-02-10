---
phase: 04-ota
plan: 02
subsystem: ota, wifi, ui
tags: [esp-ota, http-server, sha256, mbedtls, wifi-sta, lvgl, progress-bar, state-machine]

# Dependency graph
requires:
  - phase: 04-ota-01
    provides: "OtaState enum, IOtaService interface, GATT OTA prov service, BLE shutdown, OTA constants"
  - phase: 02-ble-core
    provides: "BleService singleton with shutdown() method"
provides:
  - "Wi-Fi STA module (non-blocking connect, polling status)"
  - "HTTP OTA server (POST /update with streaming 4KB writes, GET /status)"
  - "SHA-256 incremental verification via mbedtls"
  - "OTA progress queue (HTTP task -> system_task)"
  - "OtaService state machine (12 states, process() driven)"
  - "OtaScreen IScreen with progress bar, percentage, bytes, state text"
affects: [04-03, main-integration, ota-flow]

# Tech tracking
tech-stack:
  added: [esp_wifi, esp_http_server, mbedtls-sha256, esp_ota_ops]
  patterns: [non-blocking-wifi-polling, heap-allocated-receive-buffer, state-machine-process-loop, progress-queue-bridge]

key-files:
  created:
    - include/services/ota/ota_wifi.h
    - include/services/ota/ota_http_server.h
    - include/services/ota/ota_service.h
    - include/ui/screens/ota_screen.h
    - src/services/ota/ota_wifi.cpp
    - src/services/ota/ota_http_server.cpp
    - src/services/ota/ota_service.cpp
    - src/ui/screens/ota_screen.cpp
  modified: []

key-decisions:
  - "Non-blocking Wi-Fi polling via xEventGroupGetBits (0 timeout) instead of blocking xEventGroupWaitBits -- avoids stalling LVGL in system_task"
  - "Heap-allocated 4KB receive buffer (malloc) instead of stack to avoid HTTP task stack overflow"
  - "SHA-256 computed incrementally per chunk via mbedtls_sha256_update during transfer, not post-transfer"
  - "OTA progress queue uses xQueueOverwrite (size 1) to always hold latest event without backpressure"
  - "OtaScreen follows SettingsScreen pattern: direct LVGL widgets, no ButtonManager, no interactive elements"
  - "Lambda callbacks in processWaitingFirmware/processReceiving capture singleton pointer for state transitions"

patterns-established:
  - "Wi-Fi STA polling pattern: connect() starts async, check_connected()/check_failed() poll result with 0 timeout"
  - "HTTP OTA upload pattern: esp_ota_begin -> chunk loop (recv+write+sha256) -> esp_ota_end -> set_boot_partition -> restart"
  - "OTA state machine: switch(state_) in process() with per-state handler methods, transitionTo() for logging + GATT notify"

# Metrics
duration: 5min
completed: 2026-02-10
---

# Phase 4 Plan 2: OTA Service Core Summary

**Wi-Fi STA non-blocking module, HTTP OTA server with streaming 4KB writes and SHA-256 verification, 12-state OtaService state machine, and OtaScreen progress bar UI**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-10T19:49:12Z
- **Completed:** 2026-02-10T19:55:00Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- Wi-Fi STA module with non-blocking connect/disconnect and polling status check
- HTTP server on port 8080 receiving firmware via POST /update in 4KB heap-allocated chunks with incremental SHA-256
- OtaService 12-state machine orchestrating full OTA flow from Wi-Fi connect through reboot
- OtaScreen with progress bar, percentage label, bytes transferred label, and Portuguese state text

## Task Commits

Each task was committed atomically:

1. **Task 1: Wi-Fi STA module + HTTP OTA server with SHA-256 + OTA progress queue** - `36f97fd` (feat)
2. **Task 2: OtaService state machine + OtaScreen UI with progress bar** - `09c01f2` (feat)

## Files Created/Modified
- `include/services/ota/ota_wifi.h` - Wi-Fi STA public API (connect, check_connected, check_failed, shutdown)
- `include/services/ota/ota_http_server.h` - HTTP server + progress queue public API
- `include/services/ota/ota_service.h` - OtaService class declaration with 12-state machine
- `include/ui/screens/ota_screen.h` - OtaScreen IScreen with progress/state/error display
- `src/services/ota/ota_wifi.cpp` - Wi-Fi STA event handler, non-blocking polling functions
- `src/services/ota/ota_http_server.cpp` - HTTP /update POST handler with SHA-256, /status GET, progress queue
- `src/services/ota/ota_service.cpp` - State machine process() with per-state handlers and GATT status notify
- `src/ui/screens/ota_screen.cpp` - LVGL progress bar, percentage, bytes, state text in Portuguese

## Decisions Made
- Non-blocking Wi-Fi via xEventGroupGetBits polling (not xEventGroupWaitBits blocking) to preserve LVGL responsiveness
- 4KB receive buffer on heap (malloc) instead of stack to avoid HTTP task stack overflow per research pitfall #3
- SHA-256 computed incrementally per chunk via mbedtls during transfer for memory efficiency
- Progress queue size 1 with xQueueOverwrite ensures latest-event-wins without queue backpressure
- OtaScreen follows SettingsScreen pattern (direct LVGL widgets, no ButtonManager) with no interactive elements per OTA-08
- Lambda callbacks in process state handlers capture singleton pointer for clean state transitions from progress queue

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- OTA service core complete with all infrastructure modules
- Plan 03 can integrate OtaService into main.cpp system_task loop
- OtaScreen ready for registration in ScreenManager
- Progress callback mechanism ready to wire OtaService -> OtaScreen
- Wi-Fi credentials from GATT provisioning (Plan 01) feed directly into OtaService::startProvisioning()

## Self-Check: PASSED

All 8 files verified present. Both task commits (36f97fd, 09c01f2) verified in git log. SUMMARY.md exists. Build passes with zero errors.

---
*Phase: 04-ota*
*Completed: 2026-02-10*
