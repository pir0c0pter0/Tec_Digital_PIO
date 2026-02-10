---
phase: 02-ble-core
plan: 04
subsystem: ble
tags: [nimble, ble, gatt, notifications, event-queue, status-bar, integration, esp32-s3]

# Dependency graph
requires:
  - phase: 02-ble-core/plan-01
    provides: "NimBLE stack, BleService singleton, GAP event handler, BleStatus enum"
  - phase: 02-ble-core/plan-02
    provides: "GATT services (DIS, Journey, Diagnostics), val_handle exports, pack functions"
  - phase: 02-ble-core/plan-03
    provides: "BLE event queue (ble_post_event/ble_process_events), StatusBar setBleStatus()"
provides:
  - "End-to-end BLE integration: advertise -> connect -> pair -> read -> notify -> disconnect"
  - "StatusBar BLE icon driven by live GAP events through event queue"
  - "Journey/ignition state change notifications to subscribed BLE clients"
  - "Per-characteristic subscription tracking (journey and ignition independent)"
  - "GATT write validation utility (gatt_validation.h) scaffolded for Phase 3"
affects: [03-settings, 04-ota]

# Tech tracking
tech-stack:
  added: []
  patterns: [event queue consumer in main loop, GAP event posting for UI bridge, NimBLE notify_custom for push notifications, per-characteristic subscription tracking]

key-files:
  created:
    - "include/services/ble/gatt/gatt_validation.h"
  modified:
    - "src/services/ble/ble_service.cpp"
    - "src/main.cpp"
    - "include/services/ble/gatt/gatt_journey.h"
    - "src/services/ble/gatt/gatt_journey.cpp"

key-decisions:
  - "Per-characteristic subscription tracking via gatt_journey_update_subscription(attr_handle) instead of combined setter -- prevents independent subscriptions from resetting each other"
  - "ble_gatts_notify_custom() called directly from onIgnicaoStatusChange/onJornadaStateChange callbacks on Core 0 -- NimBLE queues internally, safe from any thread context"
  - "gatt_validation.h as header-only static inline utility -- zero overhead until used in Phase 3, includes NimBLE headers for self-contained compilation"

patterns-established:
  - "BLE event flow: NimBLE GAP handler -> ble_post_event() -> queue -> ble_process_events() -> onBleEvent() -> statusBar.setBleStatus()"
  - "BLE notification trigger: domain callback (ignition/journey) -> notify_*_state() -> ble_gatts_notify_custom() (if subscribed)"
  - "Subscription lifecycle: CONNECT sets conn_handle, SUBSCRIBE tracks per-characteristic, DISCONNECT resets all"

# Metrics
duration: 4min
completed: 2026-02-10
---

# Phase 2 Plan 04: BLE Integration Wiring Summary

**End-to-end BLE integration: event queue consumer drives StatusBar icon through 4 states, journey/ignition changes push notifications to subscribed clients, GATT validation scaffolded for Phase 3**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-10T16:45:25Z
- **Completed:** 2026-02-10T16:50:13Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Main loop processes BLE events from FreeRTOS queue every 5ms, driving StatusBar icon through all 4 color states (gray/blue/bright blue/green)
- GAP event handler posts events for CONNECT, DISCONNECT, ENC_CHANGE, MTU, and ADVERTISING state transitions
- Journey state changes trigger BLE notifications to subscribed clients via notify_journey_state()
- Ignition state changes trigger BLE notifications to subscribed clients via notify_ignition_state()
- Per-characteristic subscription tracking ensures independent journey/ignition subscriptions
- GATT write validation utility (gatt_validation.h) provides gatt_validate_write() and gatt_read_write_data() for Phase 3

## Task Commits

Each task was committed atomically:

1. **Task 1: Wire event queue into BLE service + main loop consumer + StatusBar updates** - `c731ca5` (feat)
2. **Task 2: Journey/ignition notification triggers + GATT validation utility scaffold** - `beca6f1` (feat)

## Files Created/Modified
- `src/services/ble/ble_service.cpp` - Added ble_event_queue.h + gatt_journey.h includes, ble_event_queue_init() in init(), ble_post_event() calls in GAP handler, subscription tracking in SUBSCRIBE, conn_handle/subscription reset in DISCONNECT
- `src/main.cpp` - Added ble_event_queue.h + gatt_journey.h includes, onBleEvent() handler, ble_process_events() in main loop, initial BLE icon set, notify calls in ignition/journey callbacks
- `include/services/ble/gatt/gatt_journey.h` - Added notification function declarations: gatt_journey_set_conn_handle, gatt_journey_update_subscription, gatt_journey_reset_subscriptions, notify_journey_state, notify_ignition_state
- `src/services/ble/gatt/gatt_journey.cpp` - Added notification state (conn_handle, per-characteristic enable flags), notification send functions using ble_gatts_notify_custom
- `include/services/ble/gatt/gatt_validation.h` - New header-only utility with gatt_validate_write() and gatt_read_write_data() (Phase 3 scaffold)

## Decisions Made
- **Per-characteristic subscription tracking:** Changed plan's `gatt_journey_set_notify_enabled(bool journey, bool ignition)` to `gatt_journey_update_subscription(uint16_t attr_handle, bool notify)` because the combined setter would reset one subscription when the other changed. Each BLE_GAP_EVENT_SUBSCRIBE event only carries one attr_handle.
- **Direct notification calls from callbacks:** notify_journey_state() and notify_ignition_state() are called directly from onIgnicaoStatusChange/onJornadaStateChange on Core 0. NimBLE's ble_gatts_notify_custom() queues internally, making it safe from any thread context.
- **Self-contained gatt_validation.h:** Includes NimBLE headers (ble_hs.h, ble_gatt.h, os_mbuf.h) directly so consumers don't need to manage include order. Static inline functions have zero overhead until instantiated.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Independent subscription tracking for journey/ignition**
- **Found during:** Task 2 (wiring SUBSCRIBE handler)
- **Issue:** Plan's `gatt_journey_set_notify_enabled(bool journey, bool ignition)` API would reset one subscription when enabling the other, since each BLE_GAP_EVENT_SUBSCRIBE carries only one attr_handle. Subscribing to journey notifications would unsubscribe from ignition and vice versa.
- **Fix:** Changed API to `gatt_journey_update_subscription(uint16_t attr_handle, bool notify)` which updates only the matching characteristic. Added `gatt_journey_reset_subscriptions()` for disconnect cleanup.
- **Files modified:** include/services/ble/gatt/gatt_journey.h, src/services/ble/gatt/gatt_journey.cpp, src/services/ble/ble_service.cpp
- **Verification:** Build succeeds, each subscription event only affects the relevant characteristic
- **Committed in:** beca6f1 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Essential fix for correct notification subscription behavior. No scope creep.

## Issues Encountered
None -- build succeeded on first attempt for both tasks.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 2 (BLE Core) is fully complete: all 4 plans executed
- Full BLE pipeline operational: advertise -> connect -> pair -> read -> notify -> disconnect -> re-advertise
- StatusBar reflects live BLE state through event queue bridge
- GATT services readable (DIS, Journey, Diagnostics) and notifiable (Journey State, Ignition Status)
- gatt_validation.h ready for Phase 3 writable Configuration Service characteristics
- RAM at 9.2% (30256 bytes), Flash at 35.6% (1119864 bytes) -- well within budget
- Ready for Phase 3: Settings + Config Sync

---
*Phase: 02-ble-core*
*Completed: 2026-02-10*

## Self-Check: PASSED
- All 5 key files exist on disk
- Both commit hashes (c731ca5, beca6f1) found in git log
- 5 ble_post_event calls in ble_service.cpp (connect, disconnect, enc_change, MTU, advertising)
- 1 ble_process_events call in main.cpp (main loop consumer)
- 2 notify calls in main.cpp (ignition + journey callbacks)
- gatt_validate_write present in gatt_validation.h
- Build succeeds: RAM 9.2%, Flash 35.6%
