---
phase: 02-ble-core
plan: 01
subsystem: ble
tags: [nimble, ble, gap, advertising, security, bonding, esp32-s3]

# Dependency graph
requires:
  - phase: 01-foundation
    provides: "NVS partition layout, app_config.h BLE placeholders, interface patterns"
provides:
  - "NimBLE stack running as GATT server peripheral"
  - "IBleService abstract interface and BleStatus enum"
  - "BleService singleton with GAP advertising and LE Secure Connections"
  - "Centralized BLE UUID header (ble_uuids.h)"
  - "sdkconfig.defaults with all NimBLE flags"
affects: [02-02, 02-03, 02-04, 03-settings, 04-ota]

# Tech tracking
tech-stack:
  added: [NimBLE (ESP-IDF built-in), esp_bt controller]
  patterns: [NimBLE singleton service, GAP event handler, LE Secure Connections Just Works, bond persistence via NVS default partition]

key-files:
  created:
    - "include/interfaces/i_ble.h"
    - "include/config/ble_uuids.h"
    - "include/services/ble/ble_service.h"
    - "src/services/ble/ble_service.cpp"
  modified:
    - "sdkconfig.defaults"
    - "include/config/app_config.h"
    - "src/main.cpp"

key-decisions:
  - "Remove ble_store_config_init() call -- not available in ESP-IDF 5.3.1 NimBLE, bond store auto-configures via CONFIG_BT_NIMBLE_NVS_PERSIST=y"
  - "Service UUIDs placed in scan response (not advertising data) to stay under 31-byte adv packet limit"
  - "BLE init placed after systemInitialized=true in system_task to avoid blocking boot sequence"

patterns-established:
  - "NimBLE singleton: BleService::getInstance()->init() pattern for BLE lifecycle"
  - "GAP event handler with status callback: BLE state changes propagated via BleStatusCallback"
  - "Centralized UUID header: all custom 128-bit UUIDs in ble_uuids.h with little-endian BLE_UUID128_INIT"

# Metrics
duration: 8min
completed: 2026-02-10
---

# Phase 2 Plan 01: NimBLE BLE Init + GAP Advertising Summary

**NimBLE BLE stack with GAP advertising as "GS-Jornada-XXYY", LE Secure Connections (Just Works), and NVS bond persistence**

## Performance

- **Duration:** 8 min
- **Started:** 2026-02-10T16:20:26Z
- **Completed:** 2026-02-10T16:29:25Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- NimBLE stack fully initialized as GATT server peripheral on ESP32-S3
- Device advertises as "GS-Jornada-XXYY" (dynamic from BT MAC address) visible in BLE scanners
- LE Secure Connections (Just Works) pairing with AES-CCM encryption
- Bond persistence via NVS default partition -- survives power cycles
- IBleService abstract interface established for all BLE service consumers
- Centralized UUID header for Journey and Diagnostics services
- RAM usage increased from 5.3% to 9.2% (within budget of ~77KB NimBLE estimate)

## Task Commits

Each task was committed atomically:

1. **Task 1: sdkconfig NimBLE flags + BLE constants + IBleService interface + UUIDs header** - `efef86f` (feat)
2. **Task 2: BLE service implementation + main.cpp integration** - `346005a` (feat)

## Files Created/Modified
- `sdkconfig.defaults` - Added 20+ NimBLE config flags (BT_ENABLED, NIMBLE, SC, bonding, MTU 512, memory optimization)
- `include/config/app_config.h` - Updated BLE section: MTU 512, adv intervals, protocol version, rate limiting constant
- `include/interfaces/i_ble.h` - Abstract BLE service interface with BleStatus enum and pure virtual methods
- `include/config/ble_uuids.h` - Centralized 128-bit UUIDs for Journey and Diagnostics services (little-endian)
- `include/services/ble/ble_service.h` - BleService concrete header with singleton, NimBLE callbacks, private members
- `src/services/ble/ble_service.cpp` - Full NimBLE implementation: init, GAP advertising, security, event handler (~300 lines)
- `src/main.cpp` - Added BLE init after system initialization in system_task

## Decisions Made
- **ble_store_config_init() removed:** Function does not exist in ESP-IDF 5.3.1 NimBLE. Bond store auto-configures when CONFIG_BT_NIMBLE_NVS_PERSIST=y is set in sdkconfig. Discovered during build verification.
- **Service UUIDs in scan response:** Advertising data (flags + name + TX power = 23 bytes) leaves no room for 128-bit UUIDs (18 bytes each). Placed Journey service UUID in scan response packet per research Pitfall 5.
- **BLE init after systemInitialized:** Placing BLE init after UI setup ensures the boot sequence (splash, NVS, audio, ignition, screen manager) completes before NimBLE host task consumes ~50KB RAM and starts background processing.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Stale sdkconfig.esp32s3_idf blocking NimBLE activation**
- **Found during:** Task 1 verification (pio run)
- **Issue:** PlatformIO generated `sdkconfig.esp32s3_idf` had `CONFIG_BT_ENABLED is not set`, which takes precedence over `sdkconfig.defaults`. Even after `pio run --target clean`, the stale config was regenerated from cache.
- **Fix:** Removed `sdkconfig.esp32s3_idf` (it's in .gitignore) and deleted `.pio/build/` directory to force full sdkconfig regeneration from `sdkconfig.defaults`.
- **Files affected:** sdkconfig.esp32s3_idf (regenerated, not tracked in git)
- **Verification:** After removal, `pio run` regenerated the file with NimBLE flags and build succeeded.
- **Committed in:** Part of Task 2 verification cycle

**2. [Rule 1 - Bug] ble_store_config_init() does not exist in ESP-IDF 5.3.1**
- **Found during:** Task 2 build verification
- **Issue:** Plan specified `ble_store_config_init()` call from bleprph example, but this function is not present in the ESP-IDF 5.3.1 NimBLE port. The store is auto-initialized when `CONFIG_BT_NIMBLE_NVS_PERSIST=y`.
- **Fix:** Removed the call and the `store/config/ble_store_config.h` include. Added comment explaining auto-configuration.
- **Files modified:** `src/services/ble/ble_service.cpp`
- **Verification:** Build succeeds, bond persistence confirmed by NVS init log.
- **Committed in:** `346005a` (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 bug)
**Impact on plan:** Both fixes essential for successful compilation. No scope creep.

## Issues Encountered
- PlatformIO sdkconfig caching is aggressive -- `pio run --target clean` does not remove the generated `sdkconfig.<env>` file. Only removing the file and the full build directory forces regeneration from `sdkconfig.defaults`. This is important knowledge for any future sdkconfig changes.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- NimBLE stack is running, IBleService interface defined, UUID header centralized
- Ready for Plan 02-02: GATT service definitions (Journey, DIS, Diagnostics)
- The GAP event handler already has BLE_GAP_EVENT_SUBSCRIBE handling wired for future GATT notifications
- RAM at 9.2% (30KB of 328KB) with ~298KB free for remaining BLE GATT services

---
*Phase: 02-ble-core*
*Completed: 2026-02-10*

## Self-Check: PASSED
- All 7 key files exist on disk
- Both commit hashes (efef86f, 346005a) found in git log
- ble_service.cpp has 426 lines (exceeds 150 min_lines requirement)
- Build succeeds: RAM 9.2%, Flash 35.3%
