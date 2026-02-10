---
phase: 04-ota
plan: 01
subsystem: ble, ota
tags: [nimble, gatt, ota, wifi-provisioning, freertos-queue, esp32]

# Dependency graph
requires:
  - phase: 02-ble-core
    provides: "NimBLE GATT server, GAP event handler, subscription tracking pattern"
  - phase: 03-settings
    provides: "Config event queue pattern, gatt_validation.h, GATT module pattern"
provides:
  - "OtaState enum (12 states) and OTA type definitions"
  - "IOtaService abstract interface"
  - "OTA Provisioning GATT service (3 characteristics: wifi_creds, status, ip_addr)"
  - "OTA provisioning event queue (BLE -> system_task)"
  - "BleService::shutdown() for NimBLE teardown"
  - "OTA Wi-Fi constants in app_config.h"
  - "OTA Provisioning UUIDs (group 4) in ble_uuids.h"
affects: [04-02, 04-03, ota-service, wifi-manager, http-server]

# Tech tracking
tech-stack:
  added: []
  patterns: [gatt-module-pattern-extended-to-ota, ble-shutdown-sequence]

key-files:
  created:
    - include/services/ota/ota_types.h
    - include/interfaces/i_ota.h
    - include/services/ble/gatt/gatt_ota_prov.h
    - src/services/ble/gatt/gatt_ota_prov.cpp
  modified:
    - include/config/app_config.h
    - include/config/ble_uuids.h
    - src/services/ble/gatt/gatt_server.cpp
    - include/services/ble/ble_service.h
    - src/services/ble/ble_service.cpp

key-decisions:
  - "nimble_port_deinit() handles controller cleanup in ESP-IDF 5.3.1 -- no separate esp_nimble_hci_and_controller_deinit call needed"
  - "Wi-Fi credentials BLE format: [1B ssid_len][ssid][1B pwd_len][pwd] -- variable length, max 98 bytes"
  - "OTA status as 2 bytes [state, error_code] -- compact for BLE notify"
  - "IP address as 4 raw bytes (network byte order) -- app parses directly"
  - "OTA prov event queue uses C pointer callback (not C++ reference) for consistency with OtaProvEvent struct"

patterns-established:
  - "OTA GATT module follows exact same pattern as gatt_config: extern C, static state, val_handle externs, subscription tracking"
  - "BLE shutdown sequence: stop adv -> disconnect -> nimble_port_stop -> nimble_port_deinit"

# Metrics
duration: 4min
completed: 2026-02-10
---

# Phase 4 Plan 1: OTA Foundation Summary

**OTA provisioning GATT service with Wi-Fi credentials write, status/IP notify characteristics, BLE shutdown method, and OTA state machine types**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-10T19:41:22Z
- **Completed:** 2026-02-10T19:46:15Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments
- OTA type system defined: OtaState enum (12 states), OtaProgressEvent, OtaWifiCredentials structs
- IOtaService abstract interface ready for implementation in Plan 02
- OTA Provisioning GATT service (3 characteristics) registered in GATT table alongside existing DIS + Journey + Diagnostics + Config services
- BleService::shutdown() implemented with proper NimBLE teardown sequence and heap monitoring
- OTA provisioning event queue bridges GATT write callbacks to system_task safely

## Task Commits

Each task was committed atomically:

1. **Task 1: OTA types, interface, constants, UUIDs, and provisioning event queue** - `149252d` (feat)
2. **Task 2: Register OTA GATT service in GATT table + BLE shutdown method** - `6911f74` (feat)

## Files Created/Modified
- `include/services/ota/ota_types.h` - OtaState enum, OtaProgressEvent, OtaWifiCredentials, OtaProvEvent structs
- `include/interfaces/i_ota.h` - IOtaService abstract interface (startProvisioning, abort, getState, process)
- `include/services/ble/gatt/gatt_ota_prov.h` - GATT OTA provisioning callbacks and event queue declarations
- `src/services/ble/gatt/gatt_ota_prov.cpp` - GATT handlers for wifi_creds write, status/ip read+notify, event queue
- `include/config/app_config.h` - Added 11 OTA Wi-Fi constants (HTTP port, timeouts, buffer sizes)
- `include/config/ble_uuids.h` - Added 4 OTA Provisioning UUIDs (group 4: 1 service + 3 characteristics)
- `src/services/ble/gatt/gatt_server.cpp` - Registered OTA Prov service in GATT table, added value handles
- `include/services/ble/ble_service.h` - Added shutdown() method declaration
- `src/services/ble/ble_service.cpp` - Implemented shutdown(), routed OTA prov events in GAP handler

## Decisions Made
- Used `nimble_port_deinit()` only (no `esp_nimble_hci_and_controller_deinit`) as ESP-IDF 5.3.1 handles controller cleanup internally via nimble_port_deinit
- Wi-Fi credentials BLE format uses length-prefixed fields: [1B ssid_len][ssid bytes][1B pwd_len][pwd bytes] for variable-length SSID/password support
- OTA status characteristic returns compact 2-byte payload [state, error_code] for efficient BLE notifications
- IP address returned as raw 4 bytes (network byte order) rather than string for compactness
- OTA prov event queue handler uses C pointer callback (`const OtaProvEvent*`) instead of C++ reference for compatibility with extern "C" linkage

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- OTA types and GATT provisioning infrastructure complete
- Plan 02 can implement OtaService (Wi-Fi connection, HTTP server, firmware download)
- Plan 03 can integrate OTA into main.cpp system_task loop
- BLE shutdown method ready for use during OTA flow transition

## Self-Check: PASSED

All 9 files verified present. Both task commits (149252d, 6911f74) verified in git log. SUMMARY.md exists. Build passes with zero errors.

---
*Phase: 04-ota*
*Completed: 2026-02-10*
