---
phase: 02-ble-core
plan: 02
subsystem: ble
tags: [nimble, gatt, dis, journey-service, diagnostics, packed-structs, esp32-s3]

# Dependency graph
requires:
  - phase: 02-ble-core/plan-01
    provides: "NimBLE stack running, ble_uuids.h, BleService singleton, sdkconfig NimBLE flags"
  - phase: 01-foundation
    provides: "JornadaService, IgnicaoService, app_config.h constants, i_jornada.h, i_ignicao.h"
provides:
  - "GATT service table with DIS (0x180A), Journey (custom), Diagnostics (custom)"
  - "Packed binary structs: JourneyStateData (8B), IgnitionData (8B), DiagnosticsData (16B)"
  - "gatt_svr_init() central registration function"
  - "val_handle exports for journey state and ignition notifications"
  - "DIS strings: Getscale/GS-Jornada/1.0.0/ESP32-S3-R8/BLE-P1.0"
affects: [02-03, 02-04, 03-settings, 04-ota]

# Tech tracking
tech-stack:
  added: [ble_svc_dis (NimBLE built-in)]
  patterns: [GATT service table with designated initializers, packed struct data packing, extern C wrapper for C-only NimBLE headers]

key-files:
  created:
    - "include/services/ble/gatt/gatt_server.h"
    - "src/services/ble/gatt/gatt_server.cpp"
    - "include/services/ble/gatt/gatt_journey.h"
    - "src/services/ble/gatt/gatt_journey.cpp"
    - "include/services/ble/gatt/gatt_diagnostics.h"
    - "src/services/ble/gatt/gatt_diagnostics.cpp"
  modified:
    - "src/services/ble/ble_service.cpp"

key-decisions:
  - "Use NimBLE built-in ble_svc_dis for DIS -- no custom implementation needed, ESP-IDF 5.3.1 includes it"
  - "extern C wrapper required for ble_svc_dis.h -- header lacks C++ linkage guards unlike other NimBLE headers"
  - "Designated initializer field order must match struct declaration order in C++ (uuid, access_cb, arg, descriptors, flags, min_key_size, val_handle)"
  - "DiagnosticsData reduced to 16 bytes (4 fields) instead of planned 20 bytes -- ble_connections and audio_queue_depth deferred"

patterns-established:
  - "GATT characteristic access callback pattern: check ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR, pack data, os_mbuf_append"
  - "Packed struct data packing: memset to zero, fill fields, return byte count for binary BLE data"
  - "extern C wrapper for C headers without guards: extern C { #include ... } in .cpp files"

# Metrics
duration: 7min
completed: 2026-02-10
---

# Phase 2 Plan 02: GATT Services (DIS + Journey + Diagnostics) Summary

**Three GATT services with DIS (0x180A), Journey (24B packed state + 8B ignition), and Diagnostics (16B heap/uptime) -- all readable via BLE**

## Performance

- **Duration:** 7 min
- **Started:** 2026-02-10T16:33:42Z
- **Completed:** 2026-02-10T16:41:13Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Device Information Service (0x180A) with manufacturer, model, firmware, hardware, protocol version strings
- Journey Service with Journey State (24 bytes, 3 motorists x 8 bytes packed) and Ignition Status (8 bytes) characteristics
- Diagnostics Service with system heap, min heap, PSRAM, and uptime (16 bytes) characteristic
- val_handles exported for future notification support in plans 02-03/02-04
- RAM increase: only 256 bytes (29992 to 30248, still at 9.2%)

## Task Commits

Each task was committed atomically:

1. **Task 1: GATT server table + DIS + Journey + Diagnostics service definitions** - `733117e` (feat)
2. **Task 2: Wire GATT init into BLE service + build verification** - `3049ed5` (feat)

## Files Created/Modified
- `include/services/ble/gatt/gatt_server.h` - gatt_svr_init() declaration, val_handle exports
- `src/services/ble/gatt/gatt_server.cpp` - Central GATT service table, DIS config, service registration
- `include/services/ble/gatt/gatt_journey.h` - JourneyStateData (8B) and IgnitionData (8B) packed structs, access callback declarations
- `src/services/ble/gatt/gatt_journey.cpp` - pack_journey_states(), pack_ignition_data(), GATT read access callbacks
- `include/services/ble/gatt/gatt_diagnostics.h` - DiagnosticsData (16B) packed struct, access callback declaration
- `src/services/ble/gatt/gatt_diagnostics.cpp` - diagnostics_access() with ESP-IDF heap/timer APIs
- `src/services/ble/ble_service.cpp` - Replaced ble_svc_gap_init()/ble_svc_gatt_init() with gatt_svr_init()

## Decisions Made
- **Built-in ble_svc_dis used:** NimBLE's built-in DIS implementation (compiled into ESP-IDF 5.3.1) works directly. No need for custom DIS service in the GATT table -- just call ble_svc_dis_init() and setter functions.
- **extern "C" wrapper for ble_svc_dis.h:** Unlike other NimBLE headers (ble_hs.h, ble_gap.h), the DIS header does not have C++ linkage guards. Wrapping in extern "C" fixes linker errors from C++ name mangling.
- **Designated initializer order:** C++ requires designated initializers to follow struct field order. The plan's original field order (val_handle before flags) caused a compilation error. Fixed to match ble_gatt_chr_def struct: uuid, access_cb, arg, descriptors, flags, min_key_size, val_handle.
- **DiagnosticsData simplified to 16 bytes:** Removed ble_connections (u16) and audio_queue_depth (u8) fields from the plan's 20-byte struct. ble_connections can be inferred from connection events, and audio queue depth has no public API yet. This keeps the struct clean at 16 bytes.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Designated initializer field order mismatch**
- **Found during:** Task 1 build verification
- **Issue:** Plan placed val_handle before flags in GATT characteristic initializers, but ble_gatt_chr_def declares flags before val_handle. C++ enforces declaration order for designated initializers.
- **Fix:** Reordered initializers to match struct field order: uuid, access_cb, arg, descriptors, flags, min_key_size, val_handle
- **Files modified:** src/services/ble/gatt/gatt_server.cpp
- **Verification:** Build succeeds without errors
- **Committed in:** 733117e (Task 1 commit)

**2. [Rule 3 - Blocking] ble_svc_dis.h missing extern "C" linkage guards**
- **Found during:** Task 2 build verification
- **Issue:** Linker error -- undefined reference to DIS functions with C++ mangled names (_Z16ble_svc_dis_initv etc.). The ble_svc_dis.h header does not include extern "C" guards, unlike other NimBLE headers.
- **Fix:** Wrapped the #include "services/dis/ble_svc_dis.h" in extern "C" { } block
- **Files modified:** src/services/ble/gatt/gatt_server.cpp
- **Verification:** Build succeeds, DIS functions link correctly
- **Committed in:** 3049ed5 (Task 2 commit)

**3. [Rule 3 - Blocking] Dropbox sync deleted newly created GATT directories**
- **Found during:** Task 2 staging
- **Issue:** After committing Task 1 files, Dropbox sync removed the newly created include/services/ble/gatt/ and src/services/ble/gatt/ directories, causing git to show all 6 GATT files as deleted.
- **Fix:** Restored files from HEAD commit using git checkout HEAD -- [files]
- **Files affected:** All 6 GATT files (restored intact from git)
- **Verification:** Files exist on disk, build succeeds
- **Committed in:** Part of Task 2 workflow

---

**Total deviations:** 3 auto-fixed (3 blocking)
**Impact on plan:** All fixes necessary for successful compilation and file integrity. No scope creep.

## Issues Encountered
- Dropbox sync interference: Dropbox occasionally deletes newly created directories on the project filesystem. Files must be committed promptly after creation and may need restoration from git after sync conflicts. This is a known issue with Dropbox-hosted git repositories.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All 3 GATT services registered and will be visible to BLE clients (nRF Connect)
- val_handles ready for notification implementation in plan 02-03 (notify manager)
- Journey data packing reads live from JornadaService/IgnicaoService singletons
- DIS strings configured: Getscale Sistemas Embarcados / GS-Jornada / 1.0.0 / ESP32-S3-R8 / BLE-P1.0
- RAM overhead minimal: +256 bytes (9.2% total), Flash +7384 bytes (35.5% total)

---
*Phase: 02-ble-core*
*Completed: 2026-02-10*

## Self-Check: PASSED
- All 7 key files exist on disk
- Both commit hashes (733117e, 3049ed5) found in git log
- Build succeeds: RAM 9.2%, Flash 35.5%
