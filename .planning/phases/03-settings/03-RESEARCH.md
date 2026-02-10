# Phase 3: Settings + Config Sync - Research

**Researched:** 2026-02-10
**Domain:** LVGL SettingsScreen UI + NimBLE GATT Configuration Service + Bidirectional Sync
**Confidence:** HIGH

## Summary

Phase 3 builds a Settings screen with volume and brightness sliders, a GATT Configuration Service with writable characteristics, and bidirectional synchronization so changes from either the touchscreen or BLE are immediately reflected on both sides and persisted in NVS.

The codebase already provides all the infrastructure this phase needs. The IScreen interface, ScreenManager, NVS persistence (volume/brightness already implemented), BLE stack with GATT server, event queue, notification patterns, and GATT write validation utility are all in place from Phases 1 and 2. The LVGL slider widget (`LV_USE_SLIDER=1`) and bar widget (`LV_USE_BAR=1`) are already enabled in `lv_conf.h`. The SettingsScreen is the first screen that does NOT wrap an existing ButtonManager/domain-logic pair -- it creates its own LVGL widgets directly on its screen object, which is simpler than JornadaScreen/NumpadScreen.

The primary technical challenge is the bidirectional sync pattern: when the user drags a slider, the device must (1) apply the change immediately, (2) persist to NVS, and (3) send a BLE notification. When a BLE client writes a new value, the device must (1) validate the write, (2) apply the change, (3) persist to NVS, and (4) update the SettingsScreen UI if it is currently active. The race condition between simultaneous local and BLE writes is handled by last-write-wins with immediate notification to both sides.

**Primary recommendation:** Follow the established GATT module pattern (gatt_journey.h/.cpp) for the new Configuration Service. Use a simple "config event queue" (extending the existing BLE event queue pattern) to safely push BLE config changes to the UI thread. The SettingsScreen should poll current values in `onEnter()` and register for config-change events to update sliders while active.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| LVGL | 8.4.0 | Slider widgets, labels, screen creation | Already in project, `LV_USE_SLIDER=1` enabled |
| NimBLE | ESP-IDF 5.3.1 bundled | GATT Configuration Service | Already initialized, GATT server operational |
| NVS Flash | ESP-IDF 5.3.1 | Persist volume, brightness, driver names | NvsManager singleton already handles volume/brightness |
| FreeRTOS | ESP-IDF 5.3.1 | Event queue for BLE-to-UI sync | Existing ble_event_queue pattern proven in Phase 2 |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| esp_timer | ESP-IDF 5.3.1 | Uptime for system info display | `esp_timer_get_time()` already used in diagnostics |
| esp_heap_caps | ESP-IDF 5.3.1 | Free memory for system info | `heap_caps_get_free_size()` already used in diagnostics |
| LEDC (BSP) | ESP-IDF 5.3.1 | Backlight brightness control | `bsp_display_brightness_set(int percent)` already exists |
| simple_audio_manager | Existing | Volume control | `setAudioVolume(int volume)` already exists |

### No New Libraries Needed

Phase 3 requires ZERO new library dependencies. Everything builds on existing infrastructure:
- LVGL slider: already compiled (`LV_USE_SLIDER=1` in lv_conf.h)
- NVS: NvsManager already has `saveVolume()`/`saveBrightness()`/`loadVolume()`/`loadBrightness()`
- BLE: GATT server, validation utility, notification pattern all proven
- Audio: `setAudioVolume()` in simple_audio_manager.h
- Backlight: `bsp_display_brightness_set()` in esp_bsp.c
- Time: `settimeofday()` is a standard POSIX function, no library needed

## Architecture Patterns

### Recommended File Structure

```
include/
  config/
    ble_uuids.h              # ADD: Configuration Service UUIDs (group 2)
    app_config.h              # ADD: NVS keys for driver names, config event type
  services/
    ble/gatt/
      gatt_config.h           # NEW: Configuration Service header
      gatt_server.h           # MODIFY: export config val_handles
  ui/screens/
    settings_screen.h         # NEW: SettingsScreen IScreen implementation
  interfaces/
    i_nvs.h                   # MODIFY: add driver name methods

src/
  services/
    ble/gatt/
      gatt_config.cpp         # NEW: Configuration Service implementation
      gatt_server.cpp         # MODIFY: add config service to GATT table
    nvs/
      nvs_manager.cpp         # MODIFY: add driver name persistence
  services/ble/
    ble_service.cpp           # MODIFY: route subscribe events to config module
  ui/screens/
    settings_screen.cpp       # NEW: SettingsScreen implementation
  main.cpp                    # MODIFY: register SettingsScreen, handle config events
```

### Pattern 1: SettingsScreen (Direct LVGL Widgets, No ButtonManager)

**What:** Unlike JornadaScreen/NumpadScreen which delegate to ButtonManager + domain logic, SettingsScreen creates LVGL widgets directly on its own `lv_obj_t* screen_`. It is a simpler screen pattern.

**When to use:** For screens with standard UI widgets (sliders, labels) that do not need a 4x3 button grid.

**Example:**
```cpp
// Source: Verified against existing JornadaScreen pattern + LVGL 8.4 docs
class SettingsScreen : public IScreen {
public:
    ScreenType getType() const override { return ScreenType::SETTINGS; }
    void create() override;
    void destroy() override;
    void update() override;       // Update system info labels (uptime, mem)
    void onEnter() override;      // Refresh slider positions from current values
    void onExit() override;       // Nothing special needed
    lv_obj_t* getLvScreen() const override { return screen_; }

private:
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* volumeSlider_ = nullptr;
    lv_obj_t* brightnessSlider_ = nullptr;
    lv_obj_t* volumeLabel_ = nullptr;
    lv_obj_t* brightnessLabel_ = nullptr;
    lv_obj_t* fwVersionLabel_ = nullptr;
    lv_obj_t* uptimeLabel_ = nullptr;
    lv_obj_t* memoryLabel_ = nullptr;
    bool created_ = false;
};
```

### Pattern 2: GATT Configuration Service (Follows gatt_journey Pattern)

**What:** New GATT service module following the exact same pattern as `gatt_journey.h/.cpp`: packed data structs, C linkage callbacks, static subscription state, notify functions.

**When to use:** For every new GATT service in this project.

**Example:**
```cpp
// Source: Existing gatt_journey.h pattern
struct __attribute__((packed)) ConfigVolumeData {
    uint8_t volume;     // 0-21
};

struct __attribute__((packed)) ConfigBrightnessData {
    uint8_t brightness; // 0-100
};

struct __attribute__((packed)) ConfigDriverNameData {
    uint8_t driver_id;  // 1-3
    char name[32];      // UTF-8, null-terminated (max 31 chars + null)
};

struct __attribute__((packed)) ConfigTimeSyncData {
    uint32_t unix_timestamp; // seconds since epoch
};

// Access callbacks (one per characteristic group)
int config_volume_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg);
int config_brightness_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt* ctxt, void* arg);
int config_driver_name_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt* ctxt, void* arg);
int config_time_sync_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt* ctxt, void* arg);

// Notification functions
void notify_config_volume(void);
void notify_config_brightness(void);
```

### Pattern 3: Bidirectional Sync via Config Event Queue

**What:** Extend the existing BLE event queue pattern to carry config change events. When BLE writes a new volume/brightness, the GATT callback posts a config event to a queue. The main loop consumes the event on Core 0, applies the change (audio/backlight), persists to NVS, and updates the SettingsScreen if active.

**When to use:** For any BLE write that needs to affect the UI or hardware.

**Why not call LVGL directly from GATT callback:** GATT access callbacks run in the NimBLE host task context. LVGL is not thread-safe -- all LVGL calls must happen on Core 0 system_task. The event queue bridges this gap, just like the existing BLE status event queue.

**Data flow -- Local change (touchscreen slider):**
```
User drags slider -> LV_EVENT_VALUE_CHANGED callback (Core 0)
  -> setAudioVolume() / bsp_display_brightness_set()  [immediate effect]
  -> NvsManager::saveVolume() / saveBrightness()       [persist]
  -> notify_config_volume() / notify_config_brightness() [BLE notification]
```

**Data flow -- Remote change (BLE write):**
```
BLE client writes volume -> config_volume_access() (NimBLE task)
  -> gatt_validate_write() [bounds check: 0-21]
  -> post config event to queue                        [thread-safe handoff]

Main loop (Core 0) processes config event:
  -> setAudioVolume() / bsp_display_brightness_set()   [immediate effect]
  -> NvsManager::saveVolume() / saveBrightness()       [persist]
  -> notify_config_volume()                            [echo back to BLE client]
  -> if SettingsScreen active: lv_slider_set_value()   [update UI]
```

### Pattern 4: GATT Write Validation (Using Existing Scaffold)

**What:** The `gatt_validation.h` utility was scaffolded in Phase 2 specifically for Phase 3 use. It provides `gatt_validate_write()` and `gatt_read_write_data()` as inline functions.

**Example:**
```cpp
// Source: include/services/ble/gatt/gatt_validation.h (already exists)
int config_volume_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t vol = current_volume;  // Read from cached state
        return os_mbuf_append(ctxt->om, &vol, 1);
    }

    // Write operation
    int rc = gatt_validate_write(ctxt, 1, 1);  // Exactly 1 byte
    if (rc != 0) return rc;

    uint8_t buf[1];
    if (gatt_read_write_data(ctxt, buf, sizeof(buf)) < 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (buf[0] > AUDIO_VOLUME_MAX) {  // Bounds check: 0-21
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;  // 0x13
    }

    // Post to config event queue for Core 0 processing
    post_config_event(CONFIG_EVT_VOLUME, buf[0]);
    return 0;
}
```

### Pattern 5: NVS Driver Name Persistence

**What:** Extend NvsManager with `saveDriverName()`/`loadDriverName()` methods using `nvs_set_str()`/`nvs_get_str()` for null-terminated UTF-8 strings up to 32 bytes.

**NVS Key scheme:** `"drv_0"`, `"drv_1"`, `"drv_2"` in the `"settings"` namespace (same as volume/brightness).

**Example:**
```cpp
// Add to INvsManager interface and NvsManager implementation
bool saveDriverName(uint8_t driverId, const char* name);  // max 31 chars + null
bool loadDriverName(uint8_t driverId, char* name, size_t maxLen);
```

### Pattern 6: System Time Sync via BLE

**What:** GATT-07 requires a write-only characteristic that accepts a 4-byte Unix timestamp. The device calls `settimeofday()` to set the system clock. No SNTP needed -- the mobile app provides the time.

**Example:**
```cpp
// Source: ESP-IDF System Time docs
#include <sys/time.h>

void set_system_time(uint32_t unix_timestamp) {
    struct timeval tv = {
        .tv_sec = (time_t)unix_timestamp,
        .tv_usec = 0,
    };
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "System time set to %lu", (unsigned long)unix_timestamp);
}
```

### Anti-Patterns to Avoid

- **Calling LVGL from GATT callbacks:** GATT access callbacks run in the NimBLE host task. Never call `lv_slider_set_value()` or any LVGL function from there. Always use the event queue.
- **Blocking NVS writes in GATT callbacks:** NVS flash writes can take 5-20ms. Do NOT call `nvs_set_*()` from within a GATT access callback. Post to the event queue and let Core 0 handle persistence.
- **Polling NVS in update():** Do NOT read NVS every 5ms tick in the screen's `update()` method. Read once in `onEnter()` and update via events.
- **Creating SettingsScreen with ButtonManager:** The Settings screen uses sliders, not a 4x3 button grid. Do not create a ButtonManager -- create LVGL widgets directly on the screen object.
- **Using `nvs_set_blob()` for driver names:** Use `nvs_set_str()` for null-terminated strings. Blob requires exact size match on read; str handles variable-length strings naturally.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| GATT write validation | Custom bounds checking | `gatt_validate_write()` from gatt_validation.h | Already scaffolded, handles op-type check + length validation |
| Volume/brightness NVS | New persistence code | Existing NvsManager `saveVolume()`/`saveBrightness()` | Already implemented and tested in Phase 1 |
| Audio volume control | Custom I2S manipulation | `setAudioVolume(int volume)` | Existing API in simple_audio_manager.h |
| Backlight control | Custom LEDC/PWM code | `bsp_display_brightness_set(int percent)` | Existing BSP function, handles LEDC duty cycle |
| BLE-to-UI bridge | Custom mutex/flag | FreeRTOS queue (extend ble_event_queue pattern) | Proven pattern from Phase 2, zero-timeout non-blocking |
| System time set | Custom RTC driver | `settimeofday()` POSIX function | Standard ESP-IDF, works without any hardware RTC |
| BLE notification send | Custom mbuf building | `ble_gatts_notify_custom()` pattern from gatt_journey.cpp | Proven pattern, handles mbuf lifecycle |
| UUID generation | Random UUIDs | Extend existing UUID scheme (group 2 for Config Service) | Consistent with ble_uuids.h UUID base pattern |

**Key insight:** Phase 3 is almost entirely an integration phase. Every low-level primitive (NVS, BLE, audio, backlight, screen lifecycle) already exists. The work is wiring them together through a new GATT service, a new screen, and the bidirectional sync logic.

## Common Pitfalls

### Pitfall 1: LVGL Slider Set-Value Feedback Loop
**What goes wrong:** Setting a slider value programmatically (from BLE event) fires `LV_EVENT_VALUE_CHANGED`, which triggers the local-change handler, which sends a BLE notification back, creating an infinite loop.
**Why it happens:** LVGL fires value-changed events for both user input and programmatic changes.
**How to avoid:** Use a flag (`updatingFromBle_ = true`) before calling `lv_slider_set_value()` from a BLE event handler. Check this flag in the event callback and skip the notification path if set. Clear the flag after the set.
**Warning signs:** Volume/brightness oscillating or notification storms in nRF Connect.

### Pitfall 2: Race Condition on Concurrent Local + BLE Writes
**What goes wrong:** User drags brightness slider while BLE client simultaneously writes a brightness value. Both paths try to save to NVS and notify.
**Why it happens:** Two independent event sources modifying the same state.
**How to avoid:** Last-write-wins is acceptable per requirements. Both paths converge on Core 0 (slider callback runs on Core 0, BLE events processed on Core 0), so there is inherent serialization. NvsManager's mutex protects the flash. The final value will be the last one applied, and a notification will be sent for it.
**Warning signs:** None expected -- the architecture naturally serializes on Core 0.

### Pitfall 3: GATT Service Table Must Be Static and Registered at Init
**What goes wrong:** Trying to add the Configuration Service after `gatt_svr_init()` has already been called.
**Why it happens:** NimBLE requires all services to be registered in a single `ble_gatts_add_svcs()` call during initialization.
**How to avoid:** Add the Configuration Service entries to the `gatt_svr_svcs[]` array in `gatt_server.cpp` before the terminator `{ 0 }`. The service table is a compile-time static array.
**Warning signs:** NimBLE returns error code from `ble_gatts_count_cfg()` or `ble_gatts_add_svcs()`.

### Pitfall 4: BLE_ATT_ERR_VALUE_NOT_ALLOWED for Out-of-Range Values
**What goes wrong:** BLE client writes volume=25 (max is 21) and gets a generic ATT error that is hard to debug.
**Why it happens:** Using wrong error code or not validating at all.
**How to avoid:** After `gatt_validate_write()` passes (correct length), do explicit bounds check: volume 0-21, brightness 0-100. Return `BLE_ATT_ERR_VALUE_NOT_ALLOWED` (0x13) for out-of-range, not `BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN` which is for wrong size.
**Warning signs:** nRF Connect shows "Error 0x13" -- this is correct behavior, not a bug.

### Pitfall 5: Notify Without Active Subscription Wastes Mbuf Memory
**What goes wrong:** Calling `ble_gatts_notify_custom()` when no client is subscribed allocates an mbuf that gets immediately freed.
**Why it happens:** Not tracking subscription state per characteristic.
**How to avoid:** Follow the gatt_journey pattern: track subscription per characteristic via `BLE_GAP_EVENT_SUBSCRIBE` handler. Check `config_volume_notify_enabled` before building mbuf.
**Warning signs:** `ble_gatts_notify_custom()` returning `BLE_HS_ENOENT` or `BLE_HS_ENOMEM`.

### Pitfall 6: Driver Name UTF-8 Without Null Terminator
**What goes wrong:** BLE client writes exactly 32 bytes of name data with no null terminator. NVS `nvs_set_str()` reads past the buffer looking for null.
**Why it happens:** BLE writes are raw bytes, not C strings.
**How to avoid:** In the GATT callback, always ensure null termination: copy the received bytes to a 33-byte buffer, force `buf[received_len] = '\0'`. Then call `nvs_set_str()`.
**Warning signs:** Garbled driver names, NVS corruption, or crash in strlen().

### Pitfall 7: SettingsScreen Update After Screen is Popped
**What goes wrong:** A BLE config event arrives while the user has navigated away from the Settings screen. The event handler tries to update slider widgets that may not exist.
**Why it happens:** The config event handler in main.cpp needs to check if SettingsScreen is currently active before calling LVGL updates.
**How to avoid:** Check `screenMgr->getCurrentScreen() == ScreenType::SETTINGS` before updating UI. Always apply the hardware change and NVS save regardless; only skip the UI update.
**Warning signs:** Crash in `lv_slider_set_value()` with null pointer.

## Code Examples

### LVGL Slider Creation (Volume)
```cpp
// Source: LVGL 8.4 docs (https://docs.lvgl.io/8.3/widgets/core/slider.html)
lv_obj_t* slider = lv_slider_create(screen_);
lv_slider_set_range(slider, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);  // 0-21
lv_slider_set_value(slider, currentVolume, LV_ANIM_OFF);
lv_obj_set_width(slider, 200);
lv_obj_add_event_cb(slider, onVolumeChanged, LV_EVENT_VALUE_CHANGED, this);
```

### LVGL Slider Event Callback
```cpp
// Source: LVGL 8.4 docs + existing project pattern (lambda capture with user_data)
static void onVolumeChanged(lv_event_t* e) {
    SettingsScreen* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (self->updatingFromBle_) return;  // Prevent feedback loop

    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);

    // 1. Apply immediately
    setAudioVolume(value);

    // 2. Persist to NVS
    NvsManager::getInstance()->saveVolume(static_cast<uint8_t>(value));

    // 3. Notify BLE client
    notify_config_volume();

    // 4. Update label
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d", (int)value);
    lv_label_set_text(self->volumeLabel_, buf);
}
```

### GATT Service Table Entry (Read + Write + Notify)
```cpp
// Source: Existing gatt_server.cpp pattern + ESP-IDF bleprph example
{
    .uuid = &BLE_UUID_CONFIG_VOLUME_CHR.u,
    .access_cb = config_volume_access,
    .arg = NULL,
    .descriptors = NULL,
    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
    .min_key_size = 0,
    .val_handle = &gatt_config_volume_val_handle,
},
```

### GATT Write Callback with Validation
```cpp
// Source: gatt_validation.h (existing) + ESP-IDF bleprph gatt_svr_write pattern
int config_volume_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle; (void)attr_handle; (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t vol = get_current_volume();
        return os_mbuf_append(ctxt->om, &vol, sizeof(vol));
    }

    // Write: validate length (exactly 1 byte)
    int rc = gatt_validate_write(ctxt, 1, 1);
    if (rc != 0) return rc;

    uint8_t buf[1];
    if (gatt_read_write_data(ctxt, buf, sizeof(buf)) < 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    // Bounds check
    if (buf[0] > AUDIO_VOLUME_MAX) {
        return 0x13; // BLE_ATT_ERR_VALUE_NOT_ALLOWED
    }

    // Post event for Core 0 processing (do NOT call LVGL or NVS here)
    post_config_event(CONFIG_EVT_VOLUME, buf[0]);
    return 0;
}
```

### Config Event Queue (Extending BLE Event Queue Pattern)
```cpp
// Source: Existing ble_event_queue.h/.cpp pattern
enum ConfigEventType : uint8_t {
    CONFIG_EVT_VOLUME = 0,
    CONFIG_EVT_BRIGHTNESS,
    CONFIG_EVT_DRIVER_NAME,
    CONFIG_EVT_TIME_SYNC,
};

struct ConfigEvent {
    ConfigEventType type;
    uint8_t driver_id;       // For driver name events (1-3)
    uint8_t value_u8;        // For volume/brightness
    uint32_t value_u32;      // For time sync (unix timestamp)
    char name[32];           // For driver name (null-terminated)
};

// Processed in main loop, same as ble_process_events()
bool config_process_events(void (*handler)(const ConfigEvent& evt));
```

### System Time Sync
```cpp
// Source: ESP-IDF System Time docs (settimeofday)
#include <sys/time.h>

int config_time_sync_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt* ctxt, void* arg) {
    (void)conn_handle; (void)attr_handle; (void)arg;

    // Write-only characteristic
    int rc = gatt_validate_write(ctxt, 4, 4);  // Exactly 4 bytes (uint32_t)
    if (rc != 0) return rc;

    uint8_t buf[4];
    if (gatt_read_write_data(ctxt, buf, sizeof(buf)) < 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    // Little-endian uint32_t Unix timestamp
    uint32_t timestamp = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    post_config_event(CONFIG_EVT_TIME_SYNC, timestamp);
    return 0;
}
```

## UUID Scheme for Configuration Service

Following the existing UUID pattern in `ble_uuids.h`:

```
UUID base: 0000XXXX-4A47-0000-4763-7365-0000000Y
  Y = 2 (Configuration Service group)

Service:     00000200-4A47-0000-4763-7365-00000002
Volume:      00000201-4A47-0000-4763-7365-00000002
Brightness:  00000202-4A47-0000-4763-7365-00000002
Driver Name: 00000203-4A47-0000-4763-7365-00000002
Time Sync:   00000204-4A47-0000-4763-7365-00000002
```

This follows the established pattern: Journey=group 1, Config=group 2, Diagnostics=group 3.

## SettingsScreen Layout (480x320, 40px status bar)

```
+--------------------------------------------------+  0px
| StatusBar (40px) -- persistent lv_layer_top()     |
+--------------------------------------------------+ 40px
|                                                    |
|  Volume                   [####|-----] 15         |
|                                                    |
|  Brilho                   [######|---] 75%        |
|                                                    |
|  ----------------------------------------         |
|                                                    |
|  Firmware: 1.0.0                                  |
|  Uptime: 01:23:45                                 |
|  Memoria livre: 120 KB                            |
|                                                    |
|  [Voltar]                                         |
+--------------------------------------------------+ 320px
```

Available height: 320 - 40 = 280px. Sliders at top (highest interaction priority), system info below (read-only), back button at bottom.

## NVS Key Map (All in "settings" Namespace)

| Key | Type | Size | Purpose | Existing? |
|-----|------|------|---------|-----------|
| `volume` | u8 | 1 byte | Audio volume 0-21 | YES (Phase 1) |
| `brightness` | u8 | 1 byte | Backlight 0-100% | YES (Phase 1) |
| `drv_0` | str | max 32 bytes | Driver 1 name | NEW |
| `drv_1` | str | max 32 bytes | Driver 2 name | NEW |
| `drv_2` | str | max 32 bytes | Driver 3 name | NEW |

All keys in `NVS_NS_SETTINGS` ("settings") namespace on `NVS_PARTITION_LABEL` ("nvs_data") partition.

## GATT Characteristic Summary

| Characteristic | UUID (short) | Properties | Data Size | Validation |
|---------------|-------------|------------|-----------|------------|
| Volume | 0x0201 | Read + Write + Notify | 1 byte (uint8_t) | 0-21 |
| Brightness | 0x0202 | Read + Write + Notify | 1 byte (uint8_t) | 0-100 |
| Driver Name | 0x0203 | Read + Write | 33 bytes (1 id + 32 name) | id: 1-3, name: valid UTF-8 |
| Time Sync | 0x0204 | Write only | 4 bytes (uint32_t LE) | > 0 (non-zero) |

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Polling NVS for config | Event-driven config sync | Phase 3 | No wasted flash reads |
| Single GATT server table | Modular service files | Phase 2 | Easy to add new services |
| Global audio volume var | NVS-backed persistent config | Phase 1 | Survives reboot |

## Open Questions

1. **BLE_ATT_ERR_VALUE_NOT_ALLOWED Constant**
   - What we know: The ATT error code for "Value Not Allowed" is 0x13 per Bluetooth Core Spec
   - What's unclear: Whether NimBLE defines a named constant for this
   - Recommendation: Define `#define BLE_ATT_ERR_VALUE_NOT_ALLOWED 0x13` in gatt_config.h if not available, or use raw 0x13

2. **Config Event Queue Capacity**
   - What we know: BLE event queue uses 8 items successfully
   - What's unclear: Whether config events need a separate queue or can reuse the existing one
   - Recommendation: Separate queue (8 items) for clean separation. Config events carry different data than BLE status events. Alternatively, could extend BleEvent with a union, but separate queue is cleaner.

3. **Driver Name Read Format**
   - What we know: Write is 1 byte driver_id + up to 32 bytes name
   - What's unclear: Whether read should return all 3 names at once or require driver_id parameter
   - Recommendation: Use a single characteristic where read returns all 3 names packed (3 * 33 = 99 bytes fits in negotiated MTU of 512). Write specifies which driver to update via the first byte.

## Sources

### Primary (HIGH confidence)
- Existing codebase analysis: `gatt_server.cpp`, `gatt_journey.cpp`, `gatt_validation.h`, `nvs_manager.cpp`, `ble_event_queue.cpp`, `main.cpp`, `jornada_screen.cpp`, `screen_manager.cpp`, `esp_bsp.c`, `simple_audio_manager.h`, `app_config.h`, `ble_uuids.h`, `lv_conf.h`, `i_screen.h`, `i_nvs.h`, `theme.h`
- LVGL 8.3/8.4 Slider documentation: [https://docs.lvgl.io/8.3/widgets/core/slider.html](https://docs.lvgl.io/8.3/widgets/core/slider.html)
- ESP-IDF bleprph GATT write example: [https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/bleprph/main/gatt_svr.c](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/bleprph/main/gatt_svr.c)

### Secondary (MEDIUM confidence)
- ESP-IDF NVS documentation (nvs_set_str/nvs_get_str): [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html)
- ESP-IDF System Time documentation (settimeofday): [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html)
- ESP-IDF BLE Data Exchange Guide: [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/ble/get-started/ble-data-exchange.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/ble/get-started/ble-data-exchange.html)

### Tertiary (LOW confidence)
- None. All findings verified against codebase or official documentation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All libraries already in use, zero new dependencies
- Architecture: HIGH - Follows established patterns from Phases 1-2, verified against codebase
- Pitfalls: HIGH - Race conditions and feedback loops are well-understood embedded patterns
- GATT write pattern: HIGH - Verified against ESP-IDF official example (bleprph) and existing gatt_journey.cpp
- LVGL slider API: HIGH - Verified against LVGL 8.3 official docs (API stable across 8.x)
- NVS string storage: MEDIUM - nvs_set_str documented but not yet used in this project (only nvs_set_u8 and nvs_set_blob used so far)
- Time sync: MEDIUM - settimeofday is standard POSIX but not yet used in this project

**Research date:** 2026-02-10
**Valid until:** 2026-03-10 (stable stack, no moving parts)
