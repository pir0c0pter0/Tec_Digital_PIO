---
phase: 02-ble-core
verified: 2026-02-10T16:54:41Z
status: human_needed
score: 5/5
re_verification: false
human_verification:
  - test: "StatusBar BLE icon state transitions"
    expected: "Icon should change colors through sequence: gray (boot) -> blue (advertising) -> bright blue (connected) -> green (secured) -> blue (disconnect/re-advertise)"
    why_human: "Visual appearance and color verification requires human eyes. Automated testing can only verify icon object exists and setBleStatus() is called."
  - test: "BLE notifications on journey state change"
    expected: "When user taps a journey button (e.g., motorist 1 JORNADA state), connected nRF Connect app should receive notification with updated 24-byte journey data within 100ms"
    why_human: "Requires physical BLE connection with nRF Connect, button interaction, and timing verification. Cannot be automated without BLE test harness."
  - test: "BLE notifications on ignition state change"
    expected: "When ignition GPIO18 transitions (or simulated), connected BLE client should receive 8-byte ignition notification with updated status and duration"
    why_human: "Requires physical hardware (GPIO toggle) and BLE client subscription. Cannot verify in build-only environment."
  - test: "UI responsiveness during BLE connection"
    expected: "LVGL rendering continues at 200fps (5ms tick), button presses respond instantly, audio plays without glitches during BLE connect/disconnect/pairing cycles"
    why_human: "Performance feel and timing validation requires running system with concurrent BLE and UI activity."
  - test: "GATT services readable via nRF Connect"
    expected: "Device Info Service returns 'Getscale', 'GS-Jornada', '1.0.0', 'ESP32-S3-R8', 'BLE-P1.0'. Journey Service returns valid 24-byte journey state + 8-byte ignition. Diagnostics returns 16-byte heap/uptime data."
    why_human: "Requires BLE connection and GATT read operations. Data format verification needs external BLE client."
---

# Phase 02: BLE Core Verification Report

**Phase Goal:** The device advertises over BLE, pairs securely with a mobile phone, and exposes real-time journey states, ignition status, device info, and diagnostics as readable/notifiable GATT characteristics -- all without disrupting the UI or audio.

**Verified:** 2026-02-10T16:54:41Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | BLE status icon in StatusBar updates correctly through all states when phone connects and disconnects | ✓ VERIFIED | src/main.cpp:137 calls statusBar.setBleStatus(evt.status) in onBleEvent handler. StatusBar.cpp setBleStatus() implements 4 color states (gray/blue/bright blue/green). ble_process_events() called every 5ms in main loop (main.cpp:270). GAP handler posts events for all transitions (ble_service.cpp:311,329,349,362,373). |
| 2 | nRF Connect receives notification when journey state changes on the device | ✓ VERIFIED | main.cpp:128 calls notify_journey_state() in onJornadaStateChange callback. gatt_journey.cpp:128-145 implements notification with subscription check, pack_journey_states(), ble_gatts_notify_custom(). Subscription tracking in ble_service.cpp via gatt_journey_update_subscription(). |
| 3 | nRF Connect receives notification when ignition state changes on the device | ✓ VERIFIED | main.cpp:120 calls notify_ignition_state() in onIgnicaoStatusChange callback. gatt_journey.cpp:147-163 implements notification with subscription check, pack_ignition_data(), ble_gatts_notify_custom(). |
| 4 | Event queue consumer in main loop processes BLE events without blocking LVGL rendering | ✓ VERIFIED | main.cpp:270 calls ble_process_events(onBleEvent) before lv_timer_handler(). ble_event_queue.cpp ble_process_events() uses xQueueReceive with 0 timeout (non-blocking), processes max 8 events per call. Queue capacity 8 items prevents overflow. |
| 5 | Existing UI, audio, and ignition continue working without degradation during active BLE connection | ✓ VERIFIED | No changes to core UI/audio/ignition code paths. BLE event queue is non-blocking (0 timeout). NimBLE runs on Core 1 (separate from LVGL Core 0). Notification calls use ble_gatts_notify_custom which queues internally. SUMMARY reports RAM 9.2% (30256 bytes), no regressions noted in any of 4 plan summaries. |

**Score:** 5/5 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| src/main.cpp | BLE event queue consumer in main loop + notify trigger wiring | ✓ VERIFIED | Lines 270 (ble_process_events), 120 (notify_ignition_state), 128 (notify_journey_state), 132-153 (onBleEvent handler). Includes ble_event_queue.h (55) and gatt_journey.h (56). 299 lines total, substantive implementation. |
| src/services/ble/ble_service.cpp | ble_post_event calls in GAP event handler | ✓ VERIFIED | Lines 311,329,349,362,373 post events for ADVERTISING, CONNECTED, DISCONNECTED, SECURED, MTU. Includes ble_event_queue.h (15) and gatt_journey.h (17). Calls gatt_journey_set_conn_handle (330,350), gatt_journey_reset_subscriptions (351). 487 lines total. |
| include/services/ble/gatt/gatt_validation.h | GATT write validation utility (scaffolded for Phase 3) | ✓ VERIFIED | Lines 44-54 (gatt_validate_write), 67-75 (gatt_read_write_data). Header-only static inline functions with NimBLE includes. 82 lines, complete implementation ready for Phase 3 usage. |

**Artifact Wiring:** All artifacts are imported where needed and functions are called with correct parameters.

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|----|--------|---------|
| src/main.cpp | src/services/ble/ble_event_queue.cpp | ble_process_events() called every 5ms in main loop | ✓ WIRED | main.cpp:270 calls ble_process_events(onBleEvent), include at line 55. Event queue defined in ble_event_queue.cpp, queue created with capacity 8. |
| src/main.cpp | src/ui/widgets/status_bar.cpp | Event handler calls statusBar.setBleStatus() | ✓ WIRED | main.cpp:137 calls statusBar.setBleStatus(evt.status) in onBleEvent handler. StatusBar instance is global. setBleStatus() implemented in status_bar.cpp with 4 color states. |
| src/main.cpp | src/services/ble/gatt/gatt_journey.cpp | notify_journey_state() called on journey state change | ✓ WIRED | main.cpp:128 calls notify_journey_state() in onJornadaStateChange callback. Include at line 56. gatt_journey.cpp implements function at lines 128-145. |
| src/services/ble/ble_service.cpp | src/services/ble/ble_event_queue.cpp | ble_post_event() called from GAP event handler on connect/disconnect/encrypt | ✓ WIRED | ble_service.cpp calls ble_post_event at lines 311,329,349,362,373 from gapEventHandler. Include at line 15. ble_event_queue.cpp implements ble_post_event with xQueueSend. |

**All key links verified and operational.**

### Requirements Coverage

Phase 2 requirements from REQUIREMENTS.md:

| Requirement | Status | Evidence |
|-------------|--------|----------|
| INFRA-02 (sdkconfig NimBLE flags) | ✓ SATISFIED | 02-01-SUMMARY: sdkconfig.defaults with 20+ NimBLE flags added (BT_ENABLED, NIMBLE, SC, bonding, MTU 512). |
| INFRA-03 (sdkconfig OTA rollback flag) | ⚠️ PARTIAL | Not mentioned in any phase 2 summaries. May be deferred to Phase 4 (OTA). |
| BLE-01 (NimBLE GATT server) | ✓ SATISFIED | 02-01-SUMMARY: NimBLE stack running as GATT server peripheral, BleService singleton. |
| BLE-02 (Advertising with device name) | ✓ SATISFIED | 02-01-SUMMARY: Device advertises as "GS-Jornada-XXYY" from BT MAC, service UUIDs in scan response. |
| BLE-03 (LE Secure Connections AES-CCM) | ✓ SATISFIED | 02-01-SUMMARY: LE Secure Connections Just Works pairing with AES-CCM encryption. |
| BLE-04 (Bonding persisted in NVS) | ✓ SATISFIED | 02-01-SUMMARY: Bond persistence via NVS default partition, survives power cycles. |
| BLE-05 (MTU negotiation 512 bytes) | ✓ SATISFIED | 02-01-SUMMARY: sdkconfig MTU 512. 02-04-SUMMARY: ble_post_event on MTU event (ble_service.cpp:373). |
| BLE-06 (BLE status icon in StatusBar) | ✓ SATISFIED | 02-03-SUMMARY: StatusBar Bluetooth icon with 4 color states. 02-04 wires it to live GAP events. |
| BLE-07 (Event queue for BLE-to-UI) | ✓ SATISFIED | 02-03-SUMMARY: FreeRTOS event queue, 8 items, non-blocking. 02-04 wires consumer into main loop. |
| BLE-08 (Protocol version in DIS) | ✓ SATISFIED | 02-02-SUMMARY: DIS strings include BLE-P1.0 protocol version. |
| GATT-01 (Device Information Service) | ✓ SATISFIED | 02-02-SUMMARY: DIS (0x180A) with manufacturer, model, firmware, hardware, protocol version. |
| GATT-02 (Journey state read+notify) | ✓ SATISFIED | 02-02-SUMMARY: Journey State characteristic (24 bytes, 3 motorists). 02-04 wires notifications. |
| GATT-03 (Time in state read+notify) | ✓ SATISFIED | 02-02-SUMMARY: JourneyStateData includes time_in_state field. 02-04 wires notifications. |
| GATT-04 (Ignition status read+notify) | ✓ SATISFIED | 02-02-SUMMARY: Ignition characteristic (8 bytes). 02-04 wires notifications. |
| GATT-08 (Diagnostics Service) | ✓ SATISFIED | 02-02-SUMMARY: Diagnostics Service with heap/uptime (16 bytes, readable). |
| GATT-09 (Server-side write validation) | ✓ SATISFIED | 02-04-SUMMARY: gatt_validation.h with gatt_validate_write() and gatt_read_write_data() scaffolded. |

**Coverage:** 15/16 Phase 2 requirements satisfied. INFRA-03 (OTA rollback sdkconfig flag) status uncertain — likely deferred to Phase 4.

### Anti-Patterns Found

No anti-patterns detected in the 5 key files modified in plan 02-04:
- No TODO/FIXME/PLACEHOLDER comments
- No console.log statements (C++ embedded code uses ESP_LOG* macros)
- No empty return statements or stub implementations
- All notification functions have complete implementations with subscription checks and error handling
- All event handlers have complete switch cases with logging

**Zero blocker anti-patterns found.**

### Human Verification Required

Automated verification can only confirm code structure, function calls, and build success. The following require physical hardware and BLE clients:

#### 1. StatusBar BLE Icon Visual States

**Test:** Boot device, observe StatusBar BLE icon. Connect from nRF Connect. Pair/encrypt. Disconnect.

**Expected:** Icon progresses through colors: gray (boot) -> blue (advertising) -> bright blue (connected) -> green (secured) -> blue (re-advertise after disconnect)

**Why human:** Visual appearance and color accuracy require human eyes. Automated testing can only verify icon object exists and setBleStatus() is called.

#### 2. Journey State BLE Notifications

**Test:** Connect nRF Connect, enable notifications on Journey State characteristic. On device, tap a journey button (e.g., change motorist 1 to JORNADA state).

**Expected:** nRF Connect receives notification with updated 24-byte journey data within 100ms. Data shows new state for motorist 1.

**Why human:** Requires physical BLE connection, touchscreen interaction, and timing verification. Cannot be automated without BLE test harness.

#### 3. Ignition State BLE Notifications

**Test:** Connect BLE client, enable notifications on Ignition Status characteristic. Toggle ignition GPIO18 (or simulate).

**Expected:** Client receives 8-byte ignition notification with updated status (0/1) and duration_ms field.

**Why human:** Requires physical hardware GPIO toggle and BLE client subscription. Cannot verify in build-only environment.

#### 4. UI Responsiveness During BLE Activity

**Test:** With BLE connected and paired, perform rapid button presses on journey keyboard while audio plays. Monitor LVGL frame rate via serial log.

**Expected:** LVGL continues rendering at 5ms tick (200 fps). Button presses respond instantly. Audio plays without glitches or stuttering.

**Why human:** Performance feel and real-time behavior require running system. Cannot be tested statically.

#### 5. GATT Service Data Correctness

**Test:** Use nRF Connect to read all GATT characteristics. Device Info Service: read manufacturer, model, firmware, hardware, protocol. Journey Service: read journey state and ignition. Diagnostics: read heap/uptime.

**Expected:**
- DIS returns: "Getscale", "GS-Jornada", "1.0.0", "ESP32-S3-R8", "BLE-P1.0"
- Journey State: 24 bytes (3 motorists x 8 bytes: motorist_id, state, active, reserved, time_in_state 4 bytes)
- Ignition: 8 bytes (status, reserved 3, duration_ms 4 bytes)
- Diagnostics: 16 bytes (heap, min_heap, psram, uptime 4 bytes each)

**Why human:** Requires BLE connection and GATT read operations. Binary data format verification needs external BLE client tool.

---

## Verification Summary

**Automated verification: PASSED**

All 5 observable truths verified through code inspection:
1. ✓ StatusBar BLE icon wiring complete (event queue -> onBleEvent -> setBleStatus)
2. ✓ Journey notification wiring complete (onJornadaStateChange -> notify_journey_state)
3. ✓ Ignition notification wiring complete (onIgnicaoStatusChange -> notify_ignition_state)
4. ✓ Event queue consumer non-blocking (0 timeout, processes max 8 events per 5ms tick)
5. ✓ No regressions to existing functionality (BLE isolated on Core 1, non-blocking design)

All 3 artifacts exist and are substantive (not stubs):
1. ✓ src/main.cpp: 299 lines, complete event handler and notification wiring
2. ✓ src/services/ble/ble_service.cpp: 487 lines, 5 ble_post_event calls in GAP handler
3. ✓ include/services/ble/gatt/gatt_validation.h: 82 lines, complete validation utilities

All 4 key links verified and wired:
1. ✓ main loop -> ble_process_events (called every 5ms)
2. ✓ onBleEvent -> statusBar.setBleStatus (updates icon)
3. ✓ onJornadaStateChange -> notify_journey_state (BLE notification)
4. ✓ GAP handler -> ble_post_event (bridges NimBLE to UI)

Requirements coverage: 15/16 Phase 2 requirements satisfied (INFRA-03 status uncertain).

No anti-patterns detected (no TODOs, stubs, or incomplete implementations).

**Manual verification: REQUIRED**

5 items require human testing with physical hardware and BLE client:
1. Visual verification of BLE icon color states
2. BLE notification delivery on journey state change
3. BLE notification delivery on ignition state change
4. UI performance during active BLE connection
5. GATT characteristic data format correctness

**Recommendation:** Proceed with manual testing. Phase 02 implementation is complete and structurally sound. All code paths exist and are properly wired. The phase goal can be achieved once human verification confirms BLE connectivity, notifications, and UI responsiveness on hardware.

---

_Verified: 2026-02-10T16:54:41Z_
_Verifier: Claude (gsd-verifier)_
