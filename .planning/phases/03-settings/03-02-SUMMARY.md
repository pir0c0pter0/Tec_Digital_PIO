---
phase: 03-settings
plan: 02
subsystem: ble
tags: [nimble, gatt, ble-config, freertos-queue, nvs]

# Dependency graph
requires:
  - phase: 02-ble-core
    provides: GATT server table, gatt_journey pattern, gatt_validation utility, ble_event_queue pattern
  - phase: 03-01
    provides: NVS driver name persistence (saveDriverName/loadDriverName)
provides:
  - GATT Configuration Service with 4 characteristics (volume, brightness, driver_name, time_sync)
  - Config event queue (FreeRTOS, 8 items) for BLE-to-Core0 configuration events
  - Configuration Service UUIDs (group 2) in ble_uuids.h
  - Notification functions for volume and brightness
affects: [03-03, ble-service, main-loop]

# Tech tracking
tech-stack:
  added: []
  patterns: [config-event-queue, gatt-write-validation, multi-characteristic-service]

key-files:
  created:
    - include/services/ble/gatt/gatt_config.h
    - src/services/ble/gatt/gatt_config.cpp
  modified:
    - include/config/ble_uuids.h
    - include/services/ble/gatt/gatt_server.h
    - src/services/ble/gatt/gatt_server.cpp

key-decisions:
  - "Config event queue follows ble_event_queue pattern (FreeRTOS, 8 items, non-blocking both sides)"
  - "Volume/brightness have read+write+notify; driver name read+write; time sync write-only"
  - "Driver name BLE format: 1 byte driver_id (1-3) + up to 32 bytes name; internally maps to 0-2"
  - "Out-of-range values rejected with BLE_ATT_ERR_VALUE_NOT_ALLOWED (0x13)"

patterns-established:
  - "Config event pattern: GATT write callbacks post ConfigEvent to queue, system_task processes via config_process_events()"
  - "Multi-overload post pattern: config_post_event (u8), config_post_event_driver (id+name), config_post_event_time (u32)"

# Metrics
duration: 8min
completed: 2026-02-10
---

# Phase 03 Plan 02: GATT Configuration Service Summary

**GATT Configuration Service with volume/brightness/driver-name/time-sync characteristics plus FreeRTOS config event queue for thread-safe BLE-to-Core0 bridging**

## Performance

- **Duration:** 8 min
- **Started:** 2026-02-10T18:13:37Z
- **Completed:** 2026-02-10T18:22:29Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- GATT Configuration Service module (gatt_config.h/.cpp) with 4 characteristics using gatt_validation.h write validation
- Config event queue (ConfigEvent struct + FreeRTOS queue) bridges GATT writes to Core 0 system_task safely
- 5 Configuration Service UUIDs (group 2) added to ble_uuids.h with correct little-endian encoding
- Configuration Service registered in GATT server table with correct field order and val_handles for notifiable characteristics

## Task Commits

Each task was committed atomically:

1. **Task 1: GATT Configuration Service module + config event queue** - `40db755` (feat)
2. **Task 2: Register Configuration Service in GATT server table** - `c1340bf` (feat)

## Files Created/Modified
- `include/services/ble/gatt/gatt_config.h` - Configuration Service header: ConfigEvent struct, ConfigEventType enum, 4 access callbacks, 2 notify functions, config event queue API, subscription management
- `src/services/ble/gatt/gatt_config.cpp` - Configuration Service implementation: write validation with bounds checking, NVS reads for current values, config event posting, notification via ble_gatts_notify_custom
- `include/config/ble_uuids.h` - 5 Configuration Service UUIDs added (group 2: service + volume + brightness + driver_name + time_sync)
- `include/services/ble/gatt/gatt_server.h` - 2 new extern val_handles (volume, brightness) + Configuration Service in doc comment
- `src/services/ble/gatt/gatt_server.cpp` - Configuration Service with 4 characteristics added to gatt_svr_svcs[] GATT table + val_handle definitions

## Decisions Made
- Config event queue uses same pattern as ble_event_queue (8 items, non-blocking xQueueSend/Receive with zero timeout)
- Volume/brightness characteristics read from NVS via NvsManager (mutex-protected, safe from NimBLE task)
- Driver name READ returns all 3 drivers packed (99 bytes: 3 * 33) for efficient single-read discovery
- Driver name WRITE uses BLE id 1-3 mapped to internal 0-2, with forced null-termination (Pitfall 6 from research)
- Time sync is write-only (BLE_ATT_ERR_READ_NOT_PERMITTED on read) since timestamp flows one-way from app to device
- BLE_ATT_ERR_VALUE_NOT_ALLOWED (0x13) already defined by NimBLE; header uses #ifndef guard for safety

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added val_handle declarations to gatt_server.h early for compilation**
- **Found during:** Task 1 (gatt_config.cpp compilation)
- **Issue:** gatt_config.cpp references gatt_config_volume_val_handle and gatt_config_brightness_val_handle which are declared in gatt_server.h (Task 2 scope). Task 1 cannot compile without them.
- **Fix:** Added extern declarations and definitions to gatt_server.h/.cpp as part of Task 1 build verification
- **Files modified:** include/services/ble/gatt/gatt_server.h, src/services/ble/gatt/gatt_server.cpp
- **Verification:** pio run succeeds after adding declarations
- **Committed in:** c1340bf (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Task 2 header changes pulled forward to enable Task 1 compilation. No scope creep. Final state matches plan specification exactly.

## Issues Encountered
- ARRAY_SIZE macro redefinition warning between app_config.h and nimble_npl_os.h -- pre-existing harmless warning, not introduced by this plan
- Plan 03-01 was partially executed before this session (NVS driver name methods committed, settings screen files present as untracked). This enabled loadDriverName usage in gatt_config.cpp driver name READ callback.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Configuration Service is discoverable via BLE scanner but events are not consumed yet
- Plan 03-03 wires config_event_queue consumer into main.cpp system_task loop
- Plan 03-03 will also wire SettingsScreen slider changes to BLE notifications

## Self-Check: PASSED

- All 5 key files exist on disk
- Both task commits (40db755, c1340bf) found in git history
- Build passes (pio run exits 0)
- All verification greps confirm expected patterns

---
*Phase: 03-settings*
*Completed: 2026-02-10*
