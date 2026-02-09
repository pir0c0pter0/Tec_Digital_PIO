# Roadmap: Teclado de Jornada Digital v2.0

## Overview

Transform the existing single-screen ESP32-S3 journey keypad into a multi-screen, BLE-connected, OTA-updatable device. The journey from v1.x to v2.0 follows a strict dependency chain: new partition table and screen infrastructure first (everything depends on these), then BLE communication (OTA depends on stable BLE), then settings integration (validates screen manager + BLE working together), then OTA firmware update (highest risk, needs everything else proven), and finally RPM display and documentation (polish after core is solid).

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation** - Partition table, screen manager, NVS persistence, and LVGL widget enablement
- [ ] **Phase 2: BLE Core** - NimBLE stack, secure connections, GATT services, and BLE-to-UI event bridge
- [ ] **Phase 3: Settings + Config Sync** - Settings screen with volume/brightness and bidirectional BLE config synchronization
- [ ] **Phase 4: OTA** - Firmware update via BLE with integrity verification, progress UI, and rollback safety
- [ ] **Phase 5: Polish** - RPM/speed screen, BLE protocol documentation, and production hardening

## Phase Details

### Phase 1: Foundation
**Goal**: The device boots on OTA-ready partitions with a working screen navigation system, persistent settings/journey data across reboots, and the existing jornada/numpad screens refactored into the new framework -- all v1.x functionality preserved.
**Depends on**: Nothing (first phase)
**Requirements**: INFRA-01, INFRA-04, INFRA-05, SCR-01, SCR-02, SCR-03, SCR-04, SCR-05, SCR-06, SCR-07, SCR-08, NVS-01, NVS-02, NVS-03
**Estimated plans**: 4
**Success Criteria** (what must be TRUE):
  1. Device boots from ota_0 partition with new partition table (ota_0, ota_1, otadata, nvs_data, spiffs) and all v1.x functionality works identically
  2. User can navigate between JornadaScreen, NumpadScreen, and back using the menu button in the status bar and back buttons on non-home screens
  3. Screen transitions are animated (slide left/right, 200-300ms) and the status bar with ignition state and timers remains visible on all screens
  4. Volume and brightness settings survive device reboot (persisted in NVS and restored on boot)
  5. Active journey states per motorist survive power loss and are correctly restored on next boot

**Key Risks**:
- Partition table change is irreversible -- requires USB reflash of all devices. Must validate with `gen_esp32part.py` before flashing
- Screen object memory leaks during navigation -- must implement proper lifecycle (create/destroy) with `lv_obj_del()` on screen exit
- button_manager.cpp decomposition (1205 lines) may break existing journey/numpad behavior -- must preserve all existing callbacks and popup logic

Plans:
- [ ] 01-01: Partition table + sdkconfig + LVGL widget enablement
- [ ] 01-02: Screen manager framework + StatusBar on lv_layer_top()
- [ ] 01-03: JornadaScreen + NumpadScreen extraction from button_manager.cpp
- [ ] 01-04: NVS persistence service (settings + journey state)

### Phase 2: BLE Core
**Goal**: The device advertises over BLE, pairs securely with a mobile phone, and exposes real-time journey states, ignition status, device info, and diagnostics as readable/notifiable GATT characteristics -- all without disrupting the UI or audio.
**Depends on**: Phase 1 (NVS needed for bond persistence, screen manager needed for BLE status icon, INFRA-05 constants)
**Requirements**: INFRA-02, INFRA-03, BLE-01, BLE-02, BLE-03, BLE-04, BLE-05, BLE-06, BLE-07, BLE-08, GATT-01, GATT-02, GATT-03, GATT-04, GATT-08, GATT-09
**Estimated plans**: 4
**Success Criteria** (what must be TRUE):
  1. Device appears as "GS-Jornada-XXXX" in a BLE scanner app (nRF Connect) with advertised service UUIDs, and pairs using LE Secure Connections with AES-CCM encryption
  2. After pairing, bond is persisted in NVS -- device auto-reconnects to the bonded phone without re-pairing after power cycle
  3. A BLE scanner can read current journey state, time-in-state, and ignition status for all 3 motorists in real time, and receives notifications when states change
  4. BLE status icon in the status bar updates correctly through all states: disconnected, advertising, connected, secured
  5. Device Info Service returns correct manufacturer, model, firmware version, and protocol version; Diagnostics Service returns live heap/uptime/queue data

**Key Risks**:
- NimBLE adds ~50KB internal SRAM -- must migrate LVGL display buffers to PSRAM before enabling BLE to avoid heap starvation
- NimBLE callbacks on wrong core can deadlock with LVGL -- must use FreeRTOS event queue for all BLE-to-UI communication
- Audio stuttering if NimBLE host competes with audio_task on Core 1 -- must set correct task priorities

Plans:
- [ ] 02-01: NimBLE stack init + GAP advertising + LE Secure Connections + bonding
- [ ] 02-02: GATT services (Device Info + Journey + Diagnostics) with read/notify
- [ ] 02-03: BLE-to-UI event queue + BLE status icon in StatusBar
- [ ] 02-04: GATT validation + protocol version + MTU negotiation

### Phase 3: Settings + Config Sync
**Goal**: Users can adjust volume and brightness from the touchscreen settings screen or from a connected mobile app via BLE, with changes instantly reflected on both sides and persisted across reboots.
**Depends on**: Phase 1 (screen manager for SettingsScreen), Phase 2 (BLE for config characteristics)
**Requirements**: CFG-01, CFG-02, CFG-03, CFG-04, CFG-05, GATT-05, GATT-06, GATT-07, NVS-04
**Estimated plans**: 3
**Success Criteria** (what must be TRUE):
  1. User can open the Settings screen from the menu, adjust volume via slider (0-21) and hear the change immediately, and adjust brightness via slider (0-100%) and see the backlight change in real time
  2. Settings screen displays current firmware version, device uptime, and free memory
  3. When a connected BLE client writes a new volume or brightness value, the device applies it immediately and the Settings screen (if active) updates to reflect the new value
  4. When the user changes volume or brightness on the touchscreen, a connected BLE client receives a notification with the updated value
  5. Driver names written via BLE Configuration Service are persisted in NVS and survive reboot

**Key Risks**:
- Race condition between simultaneous local UI change and BLE write for the same setting -- mitigated by last-write-wins with notification to both sides
- GATT write validation must reject out-of-range values (volume > 21, brightness > 100) to prevent undefined behavior

Plans:
- [ ] 03-01: SettingsScreen UI (volume slider, brightness slider, system info)
- [ ] 03-02: GATT Configuration Service (volume, brightness, driver names, time sync)
- [ ] 03-03: Bidirectional sync (local UI <-> BLE) + NVS persistence of all config

### Phase 4: OTA
**Goal**: Fleet operators can update device firmware wirelessly via BLE from a mobile app, with progress indication, integrity verification, and automatic rollback if the new firmware fails to boot -- no device is ever bricked by a bad update.
**Depends on**: Phase 1 (OTA partitions, screen manager for OtaScreen), Phase 2 (stable BLE for data transfer)
**Requirements**: OTA-01, OTA-02, OTA-03, OTA-04, OTA-05, OTA-06, OTA-07, OTA-08
**Estimated plans**: 3
**Success Criteria** (what must be TRUE):
  1. A mobile app can initiate OTA, transfer a complete firmware image over BLE in chunks (write-without-response at negotiated MTU), and the device writes it to the inactive OTA partition
  2. OtaScreen shows a progress bar with percentage complete, bytes received vs total, and the UI is locked during transfer to prevent accidental interruption
  3. Device verifies SHA-256 hash of received firmware image before committing -- a corrupted image is rejected and the device remains on current firmware
  4. After successful OTA and reboot, the new firmware runs a self-test (BLE init, LVGL init, touch responsive) and only then marks itself as valid; if self-test does not complete within 60 seconds, watchdog triggers automatic rollback to previous firmware
  5. OTA state machine transitions are observable via BLE status characteristic: IDLE -> PREPARING -> RECEIVING -> VERIFYING -> REBOOTING (and ABORTING/FAILED on error)

**Key Risks**:
- BLE connection drop during transfer loses all progress -- consider implementing resume-from-offset in the protocol design
- Blocking flash writes in NimBLE context causes BLE timeouts -- must buffer chunks in RAM and flush to flash from system_task
- A bug in OTA or self-test code can brick devices -- must test full OTA cycle (ota_0 -> ota_1 -> ota_0) and deliberate rollback on development hardware

Plans:
- [ ] 04-01: OTA GATT Service + OTA state machine + chunk buffering
- [ ] 04-02: OtaScreen with progress bar + UI lock during transfer
- [ ] 04-03: SHA-256 verification + self-test + rollback watchdog

### Phase 5: Polish
**Goal**: The device displays real-time RPM and speed gauges (with placeholder data ready for an external OBD adapter), and the BLE protocol is fully documented for the mobile app development team.
**Depends on**: Phase 2 (BLE for RPM data characteristics), Phase 3 (NVS for alert thresholds)
**Requirements**: RPM-01, RPM-02, RPM-03, RPM-04, DOC-01, DOC-02, DOC-03
**Estimated plans**: 3
**Success Criteria** (what must be TRUE):
  1. User can navigate to the RPM screen and see gauge visualizations for RPM and speed using lv_meter widgets, displaying placeholder/zero values when no data source is connected
  2. An external BLE device (OBD adapter) can write RPM and speed values to GATT characteristics, and the gauges update in real time
  3. RPM and speed alert thresholds are configurable via BLE, persisted in NVS, and the device can trigger alerts when thresholds are exceeded
  4. BLE protocol documentation is complete: all GATT service/characteristic UUIDs, data formats (byte order, field sizes, valid ranges), notification frequencies, and error codes
  5. Documentation includes protocol version, compatibility guide for the mobile app team, and complete OTA flow documentation (operation sequence, error handling, MTU limits)

**Key Risks**:
- RPM/speed gauges require lv_meter and lv_arc widgets (enabled in Phase 1 via INFRA-04) -- verify they render correctly on the 480x320 display
- Documentation must be accurate and complete enough for an independent team to build the mobile app without access to the firmware source

Plans:
- [ ] 05-01: RPMScreen with lv_meter gauges + BLE data characteristics
- [ ] 05-02: RPM/speed alert thresholds (NVS + BLE configurable)
- [ ] 05-03: BLE protocol documentation + OTA flow documentation

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|---------------|--------|-----------|
| 1. Foundation | 0/4 | Not started | - |
| 2. BLE Core | 0/4 | Not started | - |
| 3. Settings + Config Sync | 0/3 | Not started | - |
| 4. OTA | 0/3 | Not started | - |
| 5. Polish | 0/3 | Not started | - |

## Requirement Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| INFRA-01 | Phase 1 | Pending |
| INFRA-02 | Phase 2 | Pending |
| INFRA-03 | Phase 2 | Pending |
| INFRA-04 | Phase 1 | Pending |
| INFRA-05 | Phase 1 | Pending |
| SCR-01 | Phase 1 | Pending |
| SCR-02 | Phase 1 | Pending |
| SCR-03 | Phase 1 | Pending |
| SCR-04 | Phase 1 | Pending |
| SCR-05 | Phase 1 | Pending |
| SCR-06 | Phase 1 | Pending |
| SCR-07 | Phase 1 | Pending |
| SCR-08 | Phase 1 | Pending |
| NVS-01 | Phase 1 | Pending |
| NVS-02 | Phase 1 | Pending |
| NVS-03 | Phase 1 | Pending |
| NVS-04 | Phase 3 | Pending |
| BLE-01 | Phase 2 | Pending |
| BLE-02 | Phase 2 | Pending |
| BLE-03 | Phase 2 | Pending |
| BLE-04 | Phase 2 | Pending |
| BLE-05 | Phase 2 | Pending |
| BLE-06 | Phase 2 | Pending |
| BLE-07 | Phase 2 | Pending |
| BLE-08 | Phase 2 | Pending |
| GATT-01 | Phase 2 | Pending |
| GATT-02 | Phase 2 | Pending |
| GATT-03 | Phase 2 | Pending |
| GATT-04 | Phase 2 | Pending |
| GATT-05 | Phase 3 | Pending |
| GATT-06 | Phase 3 | Pending |
| GATT-07 | Phase 3 | Pending |
| GATT-08 | Phase 2 | Pending |
| GATT-09 | Phase 2 | Pending |
| CFG-01 | Phase 3 | Pending |
| CFG-02 | Phase 3 | Pending |
| CFG-03 | Phase 3 | Pending |
| CFG-04 | Phase 3 | Pending |
| CFG-05 | Phase 3 | Pending |
| RPM-01 | Phase 5 | Pending |
| RPM-02 | Phase 5 | Pending |
| RPM-03 | Phase 5 | Pending |
| RPM-04 | Phase 5 | Pending |
| OTA-01 | Phase 4 | Pending |
| OTA-02 | Phase 4 | Pending |
| OTA-03 | Phase 4 | Pending |
| OTA-04 | Phase 4 | Pending |
| OTA-05 | Phase 4 | Pending |
| OTA-06 | Phase 4 | Pending |
| OTA-07 | Phase 4 | Pending |
| OTA-08 | Phase 4 | Pending |
| DOC-01 | Phase 5 | Pending |
| DOC-02 | Phase 5 | Pending |
| DOC-03 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 54 total (9 categories: INFRA, SCR, NVS, BLE, GATT, CFG, RPM, OTA, DOC)
- Mapped to phases: 54
- Unmapped: 0
