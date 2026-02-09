# Feature Landscape

**Domain:** Embedded BLE device with touchscreen UI (fleet driver journey tracker)
**Researched:** 2026-02-09
**Overall confidence:** MEDIUM (training data only; web verification tools were unavailable)

## Table Stakes

Features users expect. Missing = product feels incomplete or unprofessional.

### BLE Communication

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| BLE GATT Server with custom services | Every BLE peripheral exposes services -- this is how mobile apps discover and interact with the device. Without GATT, the BLE stack is useless | Med | Use NimBLE stack (not Bluedroid). Define custom 128-bit UUIDs per service. ESP-IDF 5.3.1 includes NimBLE natively. NimBLE uses ~200KB flash and ~30KB RAM vs Bluedroid's ~350KB/~60KB |
| Device discovery and advertising | Mobile app needs to find the device. Advertising with device name and service UUIDs is the minimum requirement for BLE peripherals | Low | Include device name ("GS-Jornada-XXXX" with last 4 of MAC), primary service UUIDs in advertising data. 100ms advertising interval during pairing, 1000ms normal to save power |
| Secure pairing (LE Secure Connections) | Fleet devices handle driver work-hour data -- regulatory compliance demands encrypted communication. Unencrypted BLE is trivially sniffable with a $20 dongle | Med | Use LESC (LE Secure Connections) with numeric comparison. Display 6-digit passkey on LCD for confirmation. ESP32-S3 NimBLE supports Security Manager Protocol natively. Bond info stored in NVS |
| Connection state indication on UI | Users must know if phone is connected or not. Without visual feedback, they tap buttons in the app and wonder why nothing happens on the device | Low | BLE icon in status bar: disconnected (gray), advertising (pulsing blue), connected (solid blue), secured/bonded (blue with lock). Use GAP event callbacks from NimBLE |
| Read device status via BLE | The entire point of connecting -- app reads ignition state, current journey state, driver info, uptime. Without this, BLE serves no purpose | Med | GATT characteristics: ignition status (read+notify, 1 byte), active driver states (read+notify per driver), device uptime (read, 4 bytes), firmware version string (read) |
| Write configuration via BLE | Fleet operators need to configure devices remotely: volume, brightness, driver names, system parameters. Without this, every config change requires physical interaction with the touchscreen | Med | GATT characteristics with write permission: volume level (1 byte, 0-21), backlight brightness (1 byte, 0-100), driver name/ID assignment (32 bytes string). Validate all writes server-side before applying |
| BLE MTU negotiation | Default BLE MTU is 23 bytes (20 payload). Log download and OTA will be unusably slow without larger MTU. Every serious BLE device negotiates higher MTU | Low | Request 512-byte MTU during connection. ESP32-S3 NimBLE supports up to 512. Actual negotiated MTU depends on phone (iOS typically 185, modern Android 512). Code must handle any negotiated value |
| Connection parameter management | Battery life on the phone matters. Devices that aggressively hold fast connections drain phone battery. Slow connection intervals make interactions feel laggy | Low | Request 30ms connection interval during active data transfer, 500ms during idle. Use connection parameter update request after initial data exchange completes |

### Screen Navigation

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Screen manager with navigation stack | Current system has swipe between 2 screens. Adding settings, RPM display, logs viewer requires proper navigation. Without a stack, there is no "back" concept | Med | Implement push/pop pattern matching `i_screen.h` interface already defined in the codebase. The `IScreen` and `IScreenManager` abstract classes are ready at `include/interfaces/i_screen.h`. Use `lv_scr_load_anim()` for transitions |
| Smooth screen transitions | Abrupt screen changes feel broken. Every touchscreen device since 2010 has animated transitions | Low | LVGL 8.4 provides `lv_scr_load_anim()` with `LV_SCR_LOAD_ANIM_MOVE_LEFT/RIGHT`, `LV_SCR_LOAD_ANIM_FADE_IN`, etc. 200-300ms transition. Set `auto_del=true` to free old screen memory. Already used in button_manager.cpp line 179 |
| Status bar persistence across screens | Ignition status and journey timers must always be visible regardless of which screen is active. Losing this context while navigating is dangerous for a safety-relevant device | Med | Use LVGL's `lv_layer_top()` for a persistent status bar overlay that renders above every screen. Alternative: screen manager re-creates the status bar component on each screen. Prefer `lv_layer_top()` for zero-flicker transitions |
| Menu button in status bar | Swipe gestures conflict with existing numpad/jornada swipe navigation. A dedicated menu button provides reliable, discoverable navigation entry point | Low | Small gear or hamburger icon in status bar right corner. Tapping opens screen selection overlay or navigates to a menu screen. Must not collide with existing ignition indicator or timers |
| Settings screen | Users need to adjust volume and brightness without connecting a phone. Every device with a screen has settings | Med | Bar/slider widgets for volume (0-21) and brightness (0-100%). System info section: firmware version, uptime, free memory. Requires `LV_USE_SLIDER=1` in `lv_conf.h` (currently 0). `LV_USE_BAR` is already enabled (1) |
| Back navigation (on-screen) | Users navigate into settings or sub-screens and need to get back. Without "back," they are trapped | Low | On-screen back arrow button in top-left corner of non-home screens. Screen manager maintains stack; back = pop. The `IScreenManager::goBack()` method is already declared in the interface |

### OTA Firmware Updates

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Dual app partition scheme | OTA without rollback = bricking risk. Every production ESP32 OTA implementation uses two app partitions so a failed update can fall back to the previous firmware | Med | Current partitions: app0=3MB factory, spiffs=1MB. Must change to dual OTA layout. 16MB flash allows generous 3MB per OTA slot. This is a BREAKING CHANGE requiring full reflash of all existing devices |
| OTA via BLE (DFU) | Device has no Wi-Fi. BLE is the only wireless channel. Fleet operators cannot physically access every device to update firmware. Serial-only updates are unacceptable for production fleet | High | Transfer firmware binary over GATT write-without-response characteristic. Chunked writes (~500 bytes per write at 512 MTU). At realistic BLE throughput (~30-50KB/s), a 2MB firmware takes 40-70 seconds |
| OTA progress indication on LCD | Firmware update takes 40-70 seconds. Without progress feedback, users think device is frozen and power-cycle it, corrupting the flash write | Low | Progress bar overlay on screen during OTA. Show percentage complete, bytes received vs total, and estimated time remaining. Disable all other UI interaction during OTA to prevent accidental interruption |
| OTA integrity verification | Corrupted firmware = bricked device. Every OTA implementation must verify integrity before committing the new image to flash | Med | ESP-IDF provides `esp_ota_ops.h` with built-in SHA-256 verification. Send expected hash as separate characteristic before transfer begins. Verify after full image received, before marking as bootable |
| OTA rollback on boot failure | If new firmware crashes on first boot, device must automatically revert to previous firmware. Devices deployed in trucks cannot be recovered if bricked | Med | ESP-IDF's `esp_ota_mark_app_valid_cancel_rollback()` API. New firmware must call this after successful initialization. If it does not call within N consecutive boot attempts, bootloader reverts to previous OTA partition |

### Data Persistence

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Journey data persistence in NVS | Power loss during operation must not lose journey state. Drivers legally need accurate work-hour records. Volatile-only storage is unacceptable for a professional fleet device | Med | Store active journey states in NVS (Non-Volatile Storage) on every state transition. NVS handles wear leveling internally. Keep write frequency low (only on state change, not periodic) to preserve flash lifetime |
| Configuration persistence in NVS | Volume, brightness, driver names, alert thresholds must survive reboots. Users will not re-configure after every power cycle | Low | Simple NVS key-value pairs. Write on change only. Read at boot to restore settings. ESP-IDF NVS API is straightforward: `nvs_set_u8()`, `nvs_get_u8()`, etc. |

## Differentiators

Features that set product apart. Not expected, but valued by fleet operators.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| RPM and speed display screen | Real-time vehicle data visualization gives drivers awareness. Competitors typically require separate gauges. Consolidating journey tracking and vehicle telemetry in one device is compelling | High | Requires receiving RPM/speed data via BLE from external vehicle OBD adapter. Use LVGL `lv_meter` widget for gauge display (needs `LV_USE_METER=1`, currently disabled). Needle animation with smooth interpolation. Defer until data source (OBD adapter) protocol is defined |
| BLE protocol documentation | Well-documented BLE protocol accelerates mobile app development. Most embedded teams ship poorly documented protocols, causing months of integration back-and-forth | Low | Document every GATT service UUID, characteristic UUID, data format (byte order, field sizes), valid ranges, notification frequency. Include protocol version field for future evolution. Publish as structured Markdown alongside firmware. This has outsized value relative to its cost |
| Journey data export via BLE | Download full journey history to phone without cloud infrastructure. Useful for auditing, compliance reporting, fleet management | Med | Bulk data transfer over BLE notify characteristic. Chunk records into MTU-sized packets with sequence numbers for reassembly. App reconstructs complete dataset. Requires journey history storage (see below) |
| Journey history log storage | Historical record of all journey state transitions. Enables compliance auditing and fleet analytics | High | Store structured records in LittleFS (circular buffer of last N records, size-limited by available flash). Each record: uint32 timestamp, uint8 driver_id, uint8 state_from, uint8 state_to, uint32 duration_ms. Binary format for efficiency |
| Configurable alert thresholds via BLE | Fleet operators customize RPM/speed alert limits per vehicle type. One size does not fit all -- a bus has different limits than a truck | Low | Store thresholds in NVS. Expose as writable GATT characteristics. Apply immediately on write. Send audio alert when threshold exceeded (audio alerts already working in v1.0) |
| Auto-reconnect BLE | Device automatically reconnects to last paired phone when in range. Driver does not need to manually reconnect every trip start | Low | Store bonding info in NVS (NimBLE handles this automatically). Enable directed advertising to known peer after disconnection. Timeout after 30s, fall back to general advertising |
| Screen dimming on inactivity | Vehicle cabin at night: bright screen is distracting and potentially dangerous. Auto-dim after configurable timeout shows operational polish | Low | Use `lv_disp_get_inactive_time()` -- already tracked by LVGL 8.4 internally. Reduce backlight PWM after timeout (backlight on GPIO1, already controlled). Wake on touch. Configurable timeout via settings screen and BLE |
| Device diagnostic info via BLE | Remote diagnostics: free heap, uptime, task stack watermarks, flash wear. Fleet operators troubleshoot without physical access to the vehicle | Low | Read-only GATT characteristics: `esp_get_free_heap_size()`, `esp_timer_get_time()`, `uxTaskGetStackHighWaterMark()`. Minimal implementation effort, high operational value for fleet maintenance |
| BLE-triggered audio test | Fleet operator sends command via BLE to play test audio -- verifies speaker works without being physically in the vehicle | Low | Write characteristic that triggers `playAudioFile()`. Reuses existing audio queue infrastructure entirely. Simple but useful for fleet maintenance workflows |
| BLE time sync | Device has no RTC battery. Timestamps drift after power loss. Syncing time from phone gives accurate journey records | Low | Single GATT write of Unix timestamp (4 bytes). Apply to ESP32 system time with `settimeofday()`. Sync on every BLE connection establishment |

## Anti-Features

Features to explicitly NOT build.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Wi-Fi connectivity | ESP32-S3 supports Wi-Fi but it is not needed. Wi-Fi consumes ~40KB RAM, requires SSID/password management UI, and adds attack surface. BLE is sufficient for device-to-phone communication | Keep BLE as sole wireless channel. If cloud connectivity is needed later, phone acts as BLE-to-cloud gateway |
| Cloud/server communication from device | Device operates in vehicle cabins with spotty cellular connectivity. Direct cloud requires Wi-Fi or cellular, adds latency, creates failure modes when offline. Phone-as-gateway is more reliable | Phone app handles cloud sync after downloading data from device via BLE. Device is offline-first |
| GPS/location tracking on device | No GPS hardware on board. Adding it increases cost and complexity. Fleet management systems typically have dedicated GPS trackers already installed in vehicles | Leave location tracking to dedicated fleet GPS units or phone GPS. Device focuses on journey state tracking |
| Complex file manager UI | Managing files on a 3.5" 480x320 touchscreen with work gloves in a truck cabin is terrible UX. File operations belong on the phone where the user has a proper keyboard and large screen | Expose data via BLE characteristics. Phone app handles file management, export, and backup |
| Multi-language UI on device | Portuguese-only is correct for the Brazilian fleet market. Adding a localization framework to embedded firmware is overhead with no immediate business value | Keep Portuguese (BR). If internationalization is needed, implement in the phone app first where it is trivially easy |
| Custom BLE protocol (non-GATT) | Raw L2CAP or custom framing protocol seems faster but breaks compatibility. Every mobile BLE framework (iOS CoreBluetooth, Android BLE API) is designed around GATT. Fighting the platform is wasteful | Use standard GATT services and characteristics. Leverage platform BLE frameworks on both iOS and Android without custom parsing layers |
| Streaming audio over BLE | BLE bandwidth (~100KB/s theoretical, ~30-50KB/s real) is insufficient for real-time audio streaming. It would be choppy and unreliable | Audio MP3 files stored locally on LittleFS (already working). BLE only triggers which file to play via a command characteristic |
| Full OBD-II protocol implementation on device | The keypad does not connect directly to the vehicle CAN bus. An external OBD/CAN adapter bridges to BLE. Implementing OBD-II parsing (hundreds of PIDs, vehicle-specific quirks) on the keypad is the wrong architecture | Receive pre-processed RPM/speed values as simple numeric BLE characteristics from an external adapter. Keep the keypad focused on display and journey tracking |
| LVGL Tabview for navigation | `lv_tabview` consumes 40-60px of screen height for tab buttons. On a 480x320 screen with a 40px status bar, losing another 40-60px to tabs leaves only 220px for content. Unacceptable | Use custom screen manager with stack-based push/pop navigation and a menu button in the status bar. Full screen area for content |
| LVGL Tileview for navigation | `lv_tileview` uses swipe gestures for navigation, which conflicts with the existing swipe-left/right between numpad and jornada keyboards. Would require redesigning the existing gesture system | Use explicit button-based navigation (menu button, back button) instead of gestures |
| LVGL Fragment module | `LV_USE_FRAGMENT=0` (disabled) and the Fragment API is C-struct-based, which is awkward for the existing C++ class architecture with `IScreen` interfaces. The project already has a cleaner abstraction | Keep `LV_USE_FRAGMENT=0`. Use the existing `IScreen`/`IScreenManager` C++ interface pattern defined in `include/interfaces/i_screen.h` |

## Feature Dependencies

```
NVS Init ──────────────────────┐
                               v
BLE NimBLE Stack Init ──> GATT Service Registration ──> BLE Advertising
     │                          │                            │
     │                          v                            v
     │                   Journey Service            Device Discovery
     │                   (read/notify)               by Mobile App
     │                          │                            │
     │                          v                            v
     │                   Config Service             Secure Pairing (LESC)
     │                   (read/write)                        │
     │                          │                            v
     │                          v                    Bond Storage (NVS)
     │                   OTA Service                         │
     │                   (write/notify)                      v
     │                          │                    Auto-Reconnect
     │                          v
     │                   OTA Data Transfer
     │                          │
     │                          v
     │                   OTA Verify (SHA-256) ──> Mark Valid / Rollback
     │
     v
Dual Partition Scheme ──> PREREQUISITE for OTA
(partition table rewrite)      (BREAKING CHANGE: requires full reflash)

Screen Manager Init ──> Register Screens ──> Navigation Stack
        │                      │
        v                      v
  Jornada Screen         Settings Screen
  (refactored from       (new, needs LV_USE_SLIDER=1)
   button_manager.cpp)         │
        │                      v
        v                Volume/Brightness/System Info
  Status Bar on                │
  lv_layer_top()               v
  (persistent across     NVS Persistence (config)
   all screens)
        │
        v
  BLE Status Icon ──> Requires BLE Stack Init
  (in status bar)

RPM/Speed Screen ──> Requires lv_meter (LV_USE_METER=1)
        │                     │
        v                     v
  Data from BLE        Alert Thresholds (NVS)
  (external OBD adapter)
```

## Critical Dependency: Partition Table Change

The current partition scheme (`app0=3MB factory, spiffs=1MB`) does NOT support OTA. Enabling OTA requires rewriting `partitions.csv`:

**Current layout (no OTA):**
```
nvs,      data, nvs,     0x9000,   0x5000     # 20KB
phy_init, data, phy,     0xE000,   0x1000     # 4KB
app0,     app,  factory, 0x10000,  0x300000   # 3MB (single app)
spiffs,   data, spiffs,  0x310000, 0x100000   # 1MB (LittleFS)
# Total used: ~4.1MB of 16MB
```

**Recommended OTA layout (16MB flash, generous):**
```
nvs,      data, nvs,      0x9000,   0x5000     # 20KB  - Settings, bonds, journey state
otadata,  data, ota,      0xE000,   0x2000     # 8KB   - OTA boot selection metadata
ota_0,    app,  ota_0,    0x10000,  0x300000   # 3MB   - Primary app slot
ota_1,    app,  ota_1,    0x310000, 0x300000   # 3MB   - OTA update slot
spiffs,   data, spiffs,   0x610000, 0x100000   # 1MB   - LittleFS (audio + splash)
# Total used: ~7.1MB of 16MB. 8.9MB headroom
```

The 16MB flash is generous enough for 3MB per OTA slot (matching current app size) plus the full 1MB LittleFS. No need to compromise on either.

**This is the single most disruptive change.** It must happen:
1. BEFORE any OTA firmware code is written
2. Ideally BEFORE BLE is added, so the first BLE-capable firmware is already on OTA-ready partitions
3. All existing devices must be reflashed via USB (partition layout changes are not OTA-upgradable)

## LVGL Widget Dependencies

Features requiring currently-disabled LVGL widgets in `include/lv_conf.h`:

| Feature | Widget Needed | Current State | Config Change | Flash Cost |
|---------|---------------|---------------|---------------|------------|
| Settings screen (volume/brightness) | `lv_slider` | `LV_USE_SLIDER=0` | Set to 1. Requires `LV_USE_BAR=1` (already enabled) | ~3-5KB |
| RPM/Speed gauge needle | `lv_meter` | `LV_USE_METER=0` | Set to 1 | ~5-8KB |
| RPM/Speed gauge arc ring | `lv_arc` | `LV_USE_ARC=0` | Set to 1 | ~3-5KB |
| Settings toggles (optional) | `lv_switch` | `LV_USE_SWITCH=0` | Set to 1 if toggle UI preferred over slider | ~2-3KB |
| Settings dropdown (optional) | `lv_dropdown` | `LV_USE_DROPDOWN=0` | Set to 1 if dropdown menus needed | ~4-6KB |

All widget enables are safe and additive. They add a small amount of flash and zero runtime RAM until instantiated.

## BLE GATT Service Architecture

Recommended GATT service layout for the firmware. This defines the API contract with the future mobile app.

### Service: Device Information (SIG Standard UUID 0x180A)

| Characteristic | UUID | Properties | Format | Description |
|---------------|------|------------|--------|-------------|
| Manufacturer Name | 0x2A29 | Read | UTF-8 string | "Getscale Sistemas Embarcados" |
| Model Number | 0x2A24 | Read | UTF-8 string | "TecladoJornada-v2" |
| Firmware Revision | 0x2A26 | Read | UTF-8 string | APP_VERSION_STRING from app_config.h |
| Hardware Revision | 0x2A27 | Read | UTF-8 string | "ESP32-S3-R8" |

### Service: Journey (Custom UUID)

| Characteristic | Properties | Format | Description |
|---------------|------------|--------|-------------|
| Driver 1 State | Read, Notify | 1 byte (EstadoJornada enum) | Current state of driver 1 |
| Driver 2 State | Read, Notify | 1 byte | Current state of driver 2 |
| Driver 3 State | Read, Notify | 1 byte | Current state of driver 3 |
| Driver 1 Time | Read, Notify | 4 bytes uint32 LE (ms) | Time in current state, driver 1 |
| Driver 2 Time | Read, Notify | 4 bytes uint32 LE | Time in current state, driver 2 |
| Driver 3 Time | Read, Notify | 4 bytes uint32 LE | Time in current state, driver 3 |
| Ignition Status | Read, Notify | 1 byte bool | 0=off, 1=on |
| Ignition Duration | Read | 4 bytes uint32 LE (ms) | Time since ignition on |

### Service: Configuration (Custom UUID)

| Characteristic | Properties | Format | Description |
|---------------|------------|--------|-------------|
| Volume Level | Read, Write | 1 byte (0-21) | Audio volume, persisted to NVS |
| Brightness | Read, Write | 1 byte (0-100) | Backlight percentage, persisted to NVS |
| Driver 1 Name | Read, Write | 32 bytes UTF-8 | Null-terminated string |
| Driver 2 Name | Read, Write | 32 bytes UTF-8 | Null-terminated string |
| Driver 3 Name | Read, Write | 32 bytes UTF-8 | Null-terminated string |
| RPM Alert Threshold | Read, Write | 2 bytes uint16 LE | RPM limit for audio alert |
| Speed Alert Threshold | Read, Write | 2 bytes uint16 LE | km/h limit for audio alert |
| System Time | Write | 4 bytes uint32 LE | Unix timestamp for clock sync |

### Service: Diagnostics (Custom UUID)

| Characteristic | Properties | Format | Description |
|---------------|------------|--------|-------------|
| Free Heap | Read | 4 bytes uint32 LE | esp_get_free_heap_size() |
| Uptime | Read | 4 bytes uint32 LE (seconds) | Since last boot |
| BLE Connection Count | Read | 2 bytes uint16 LE | Lifetime connections |
| Audio Queue Depth | Read | 1 byte (0-4) | Current audio queue utilization |
| NVS Free Entries | Read | 2 bytes uint16 LE | Remaining NVS storage |

### Service: OTA (Custom UUID)

| Characteristic | Properties | Format | Description |
|---------------|------------|--------|-------------|
| OTA Control | Write | 1 byte command | 0x01=Start, 0x02=Abort, 0x03=Verify, 0x04=Reboot |
| OTA Data | Write Without Response | Variable (up to MTU-3) | Chunked firmware binary |
| OTA Status | Read, Notify | 2 bytes: state(1) + progress(1) | State: idle/receiving/verifying/complete/error. Progress: 0-100% |
| OTA Image Size | Write | 4 bytes uint32 LE | Total firmware image size in bytes (sent before transfer) |
| OTA Image Hash | Write | 32 bytes | SHA-256 hash of complete firmware image (sent before transfer) |

## MVP Recommendation

Prioritize in this order:

1. **Partition table change** -- Prerequisite for OTA. Breaking change requiring USB reflash of all devices. Must happen first so all subsequent firmware ships on OTA-ready partitions. Low code effort but high operational impact.

2. **Screen manager framework** -- Enables all subsequent UI screens. Implement the `IScreen`/`IScreenManager` interfaces already declared in `include/interfaces/i_screen.h`. Refactor `button_manager.cpp` (1205 lines) into a `JornadaScreen` class. Move status bar to `lv_layer_top()`. Without this, settings screen and RPM screen cannot be added.

3. **Settings screen** -- Proves screen manager works. Immediate user value (volume/brightness without needing a phone). Low risk. Enable `LV_USE_SLIDER=1` in `lv_conf.h`.

4. **NVS persistence** -- Must happen before BLE config writes. Store volume, brightness, driver names, journey state. Critical for data integrity across power cycles.

5. **BLE GATT server with secure pairing** -- Core wireless infrastructure. Start with Device Info Service + Journey Service (read-only notifications). Get pairing, encryption, and bonding working before adding writable characteristics.

6. **Configuration via BLE (write characteristics)** -- Once GATT server works, add write permissions to Configuration Service. Validate writes, persist to NVS, apply in real-time.

7. **OTA via BLE** -- Highest complexity. Build on working BLE stack and dual partitions. Implement chunked transfer, integrity verification, progress UI, and rollback. Test extensively: partial transfer, corrupt image, power loss during write, rollback after crash.

**Defer to subsequent milestone:**
- RPM/Speed display: Requires external OBD adapter with defined BLE protocol (not yet available). Build the meter screen infrastructure but defer data integration.
- Journey data export: Depends on both working BLE and journey history storage. Add after core BLE features are validated.
- Journey history log storage: Complex circular buffer with flash management. Build after NVS persistence for config is proven.

## Resource Budget Estimates

| Feature | Flash Impact | RAM Impact | Task/Core |
|---------|-------------|------------|-----------|
| NimBLE stack | ~200-300KB flash | ~30-50KB RAM (internal SRAM required) | NimBLE host task (Core 0, priority 21 default) |
| GATT services (5 services, ~20 characteristics) | ~20-40KB flash | ~5-10KB RAM | Runs within NimBLE task context |
| Screen manager + navigation stack | ~5-10KB flash | ~2-4KB RAM (screen pointer stack) | Core 0 (LVGL context, existing system_task) |
| Settings screen widgets | ~10-15KB flash | ~3-5KB RAM (only when screen active) | Core 0 (LVGL context) |
| RPM/Speed screen with meter | ~15-20KB flash | ~5-8KB RAM (only when screen active) | Core 0 (LVGL context) |
| OTA transfer logic | ~10-15KB flash | ~8-16KB RAM (write buffer for flash programming) | Core 0 or dedicated OTA task |
| NVS persistence code | ~5KB flash | ~1-2KB RAM | No dedicated task, called from service context |
| LVGL widgets (slider, meter, arc) | ~8-15KB flash total | 0 until instantiated | Core 0 |

**Total estimated addition:** ~275-430KB flash, ~55-95KB RAM

With 3MB per OTA partition slot (recommended layout), the firmware has ample room. Current firmware size should be well under 1MB. Adding BLE + all features should remain under 2MB comfortably.

RAM budget: ESP32-S3 has 512KB internal SRAM. NimBLE requires ~30-50KB of internal RAM (BLE controller needs it). With PSRAM available for display buffers and non-time-critical allocations, the internal RAM budget should accommodate BLE. Monitor with `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` after BLE init to verify.

## Sources

- LVGL 8.4 documentation read directly from `lib/lvgl/docs/` in the project (HIGH confidence: `overview/display.md`, `overview/object.md`, `others/fragment.md`, `widgets/extra/tileview.md`, `widgets/extra/tabview.md`, `widgets/extra/meter.md`, `widgets/extra/chart.md`)
- `include/lv_conf.h` analysis of enabled/disabled widgets (HIGH confidence: read directly)
- `include/interfaces/i_screen.h` existing screen navigation interface design (HIGH confidence: read directly)
- `include/interfaces/i_jornada.h` journey state data structures (HIGH confidence: read directly)
- `partitions.csv` current partition layout (HIGH confidence: read directly)
- `include/config/app_config.h` system constants and pin assignments (HIGH confidence: read directly)
- `.planning/PROJECT.md` validated/active requirements (HIGH confidence: read directly)
- `.planning/codebase/CONCERNS.md` identified technical debt (HIGH confidence: read directly)
- `.planning/codebase/ARCHITECTURE.md` system architecture analysis (HIGH confidence: read directly)
- ESP-IDF 5.3.1 NimBLE, OTA, and NVS APIs (MEDIUM confidence: training data, not verified against v5.3.1 docs directly)
- BLE MTU negotiation behavior and throughput estimates (MEDIUM confidence: training data, real-world performance varies significantly by phone model and OS version)
- Flash/RAM budget estimates for NimBLE and OTA (MEDIUM confidence: training data, actual measurements needed post-implementation)

---

*Feature landscape analysis: 2026-02-09*
