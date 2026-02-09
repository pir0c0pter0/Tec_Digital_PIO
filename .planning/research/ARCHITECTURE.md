# Architecture Patterns: BLE + Screen Manager + OTA

**Domain:** ESP32-S3 embedded firmware with BLE, multi-screen LVGL UI, and OTA
**Researched:** 2026-02-09
**Overall confidence:** MEDIUM (BLE/NimBLE patterns from ESP-IDF training data, OTA from ESP-IDF API knowledge, screen manager patterns from LVGL community practice and existing i_screen.h; web verification unavailable)

## Recommended Architecture

### High-Level System Architecture

```
                    +-----------------------+
                    |      Mobile App       |
                    |  (Android/iOS, BLE)   |
                    +-----------+-----------+
                                |
                          BLE 5.0 (GATT)
                                |
+---------------------------------------------------------------+
|  ESP32-S3 Firmware                                             |
|                                                                |
|  +------------------+    +------------------+                  |
|  | Core 0           |    | Core 1           |                  |
|  |                  |    |                  |                  |
|  | system_task (5)  |    | audio_task (5)   |                  |
|  |  - LVGL render   |    |  - MP3 decode    |                  |
|  |  - Touch input   |    |  - I2S playback  |                  |
|  |  - Screen Mgr    |    |                  |                  |
|  |  - BLE event Q   |    +------------------+                  |
|  |                  |                                          |
|  | nimble_host (4)  |                                          |
|  |  - BLE stack     |                                          |
|  |  - GATT server   |                                          |
|  |  - Security mgr  |                                          |
|  |                  |                                          |
|  | IgnicaoMon (2)   |                                          |
|  |  - GPIO18 poll   |                                          |
|  +------------------+                                          |
|                                                                |
|  +---------------------------+  +---------------------------+  |
|  | Services Layer            |  | Data Layer                |  |
|  |                          |  |                           |  |
|  | BleService               |  | NVS (settings, bonds)    |  |
|  | IgnicaoService           |  | LittleFS (audio, logs)   |  |
|  | JornadaService           |  | OTA partitions (2x 3MB)  |  |
|  | AudioManager             |  |                           |  |
|  | OtaService               |  |                           |  |
|  | NvsManager               |  |                           |  |
|  +---------------------------+  +---------------------------+  |
|                                                                |
|  +---------------------------+                                 |
|  | UI Layer                  |                                 |
|  |                          |                                 |
|  | ScreenManager             |                                 |
|  |  +-- JornadaScreen       |                                 |
|  |  +-- NumpadScreen        |                                 |
|  |  +-- SettingsScreen      |                                 |
|  |  +-- RPMScreen           |                                 |
|  |  +-- OtaScreen           |                                 |
|  | StatusBar (+ menu btn)    |                                 |
|  | Theme                     |                                 |
|  +---------------------------+                                 |
|                                                                |
|  +---------------------------+                                 |
|  | HAL / Drivers             |                                 |
|  |  LCD (QSPI), Touch (I2C) |                                 |
|  |  I2S, GPIO, NimBLE HCI   |                                 |
|  +---------------------------+                                 |
+---------------------------------------------------------------+
```

### Core Distribution (Extended)

| Core | Existing Tasks | New Tasks |
|------|----------------|-----------|
| Core 0 | system_task (pri 5): LVGL 5ms tick, screen updates, status bar | NimBLE Host (pri 4): BLE event processing; BLE event queue consumer in system_task |
| Core 0 | IgnicaoMonitor (pri 2): GPIO18 polling every 100ms | Unchanged |
| Core 1 | audio_task (pri 5): minimp3 decode + I2S write | Unchanged -- audio remains isolated |

**Rationale for BLE on Core 0:** NimBLE's host task is event-driven and cooperative. It processes BLE events in microseconds and yields. It does NOT need its own dedicated core. Putting BLE on Core 1 would conflict with audio's real-time I2S timing requirements, which are far more timing-sensitive. NimBLE on Core 0 is the ESP-IDF default and works well alongside LVGL.

## Component Boundaries

### 1. Screen Manager (Core 0, LVGL thread)

| Property | Value |
|----------|-------|
| **Responsibility** | Navigate between screens using a stack pattern (push/pop), manage screen lifecycle |
| **Location** | `src/ui/screens/screen_manager.cpp`, `include/ui/screens/screen_manager.h` |
| **Communicates With** | All IScreen implementations, StatusBar, main.cpp (via update call in system_task loop) |
| **Thread Affinity** | Core 0 only (LVGL single-threaded requirement) |
| **Owns** | Screen stack (fixed array, max depth 8), current active screen, screen registry |

The existing `IScreen` and `IScreenManager` interfaces in `include/interfaces/i_screen.h` already define the right abstractions. The concrete ScreenManager implementation should:

- Maintain a stack (fixed-size array, max depth 8) for navigation history
- Call `onExit()` on current screen before push, `onEnter()` on new screen after push
- Use `lv_scr_load_anim()` for smooth screen transitions
- Call `update()` on the active screen from system_task's main loop

**Add to ScreenType enum (i_screen.h):**
```cpp
enum class ScreenType : uint8_t {
    SPLASH = 0,
    NUMPAD,
    JORNADA,
    SETTINGS,
    RPM_SPEED,      // NEW
    OTA_PROGRESS,    // NEW
    MAX_SCREENS
};
```

**Screen implementations to create:**

| Screen | File | Purpose | Extracted From |
|--------|------|---------|----------------|
| NumpadScreen | `src/ui/screens/numpad_screen.cpp` | Number input | numpad_example.cpp |
| JornadaScreen | `src/ui/screens/jornada_screen.cpp` | Journey button grid | button_manager.cpp + jornada_keyboard.cpp |
| SettingsScreen | `src/ui/screens/settings_screen.cpp` | Volume, brightness, BLE pairing | New |
| RPMScreen | `src/ui/screens/rpm_screen.cpp` | Real-time RPM/velocity (BLE data) | New |
| OtaScreen | `src/ui/screens/ota_screen.cpp` | OTA progress display | New |

**Menu integration with StatusBar:**

Add a hamburger/menu button to the left side of StatusBar. On tap, it pushes SettingsScreen or shows a navigation overlay. This replaces swipe-based navigation for screen switching -- swipe can remain for sub-screen navigation within a screen if needed.

### 2. BLE Service (NimBLE, Core 0)

| Property | Value |
|----------|-------|
| **Responsibility** | Manage BLE GAP/GATT, handle connections, expose data services, receive commands |
| **Location** | `src/services/ble/ble_service.cpp`, `include/services/ble/ble_service.h` |
| **Interface** | `include/interfaces/i_ble.h` |
| **Communicates With** | JornadaService (reads state), IgnicaoService (reads state), OtaService (triggers OTA), ScreenManager (status updates via queue) |
| **Thread Affinity** | NimBLE Host Task on Core 0 (ESP-IDF default), callbacks execute in NimBLE context |
| **Owns** | Connection state, GATT server, advertising, security/pairing |

**NimBLE architecture in ESP-IDF:**

1. **NimBLE Host Task** -- FreeRTOS task created by `nimble_port_freertos_init()`. Processes HCI events. Core 0. Stack ~4-6KB.
2. **BLE Controller** -- separate internal task managed by ESP-IDF (priority 22, automatic).
3. **Application callbacks** -- registered via `ble_hs_cfg` and execute in NimBLE Host Task context.

**GATT Service Layout:**

```
BleService (singleton)
  |
  +-- GAP: Advertising, connection management, pairing (LE Secure Connections)
  |
  +-- GATT Server:
  |     |
  |     +-- Journey Service (custom 128-bit UUID)
  |     |     +-- Char: Current State (read, notify) -- per-driver state
  |     |     +-- Char: Driver Info (read) -- active driver list
  |     |     +-- Char: Journey History (read) -- paginated log
  |     |
  |     +-- Device Service (custom 128-bit UUID)
  |     |     +-- Char: Ignition Status (read, notify) -- on/off + duration
  |     |     +-- Char: Device Info (read) -- firmware version, protocol version
  |     |     +-- Char: Configuration (read, write) -- volume, brightness, etc.
  |     |
  |     +-- OTA Service (custom 128-bit UUID)
  |           +-- Char: Firmware Version (read)
  |           +-- Char: OTA Control (write) -- start/abort/commit
  |           +-- Char: OTA Data (write without response) -- firmware chunks
  |           +-- Char: OTA Status (read, notify) -- progress/result
  |
  +-- Security: LE Secure Connections (LESC), AES-CCM, NVS-persisted bonds
```

**Key design decisions:**

1. **BleService does NOT directly access service internals.** Reads via public getters (getStatus, getMotorista), writes via public methods (iniciarEstado, setVolume). Services remain single source of truth.

2. **Notifications use polling, not tight coupling.** A lightweight timer (1-2s) checks if service state changed since last notification. Sends BLE notifications only on change.

3. **Protocol version in Device Info characteristic.** First operation after BLE connect should be version exchange to detect firmware/app protocol mismatches.

4. **Thread safety between NimBLE callbacks and LVGL.** NimBLE callbacks run in NimBLE Host Task (different from system_task). UI updates from BLE MUST go through a FreeRTOS queue -- never call LVGL directly from NimBLE callbacks.

### 3. BLE-to-UI Communication (Critical Pattern)

```
NimBLE callback (Core 0, NimBLE Host Task)
    |
    v
FreeRTOS Queue: appEventQueue (max 16 events, ~48 bytes per event)
    |
    v
system_task main loop (Core 0, system_task) polls queue each iteration
    |
    v
ScreenManager::handleAppEvent() -> updates active screen via LVGL
```

**Event structure:**

```cpp
enum class AppEvent : uint8_t {
    BLE_CONNECTED,
    BLE_DISCONNECTED,
    BLE_CONFIG_CHANGED,
    BLE_JOURNEY_COMMAND,
    OTA_STARTED,
    OTA_PROGRESS,
    OTA_COMPLETE,
    OTA_FAILED,
    JORNADA_STATE_CHANGED,
    IGNICAO_STATE_CHANGED,
};

struct AppEventData {
    AppEvent type;
    union {
        bool boolValue;                              // BLE connected/disconnected, ignicao
        struct { int motoristId; uint8_t state; } journey;
        uint8_t otaProgress;                         // 0-100
        struct { uint8_t key; uint32_t value; } config;
    } data;
};

// Queue created in main.cpp:
static QueueHandle_t appEventQueue = xQueueCreate(16, sizeof(AppEventData));

// NimBLE callback (producer):
AppEventData evt = {.type = AppEvent::BLE_CONNECTED, .data = {.boolValue = true}};
xQueueSend(appEventQueue, &evt, 0);  // non-blocking, drop if full

// system_task loop (consumer):
AppEventData evt;
while (xQueueReceive(appEventQueue, &evt, 0) == pdTRUE) {
    screenManager->handleAppEvent(evt);
}
lv_timer_handler();
vTaskDelay(pdMS_TO_TICKS(5));
```

This queue-based decoupling is mandatory because LVGL is NOT thread-safe -- even though both NimBLE and LVGL run on Core 0, they run in different FreeRTOS tasks.

### 4. OTA Service (Core 0, triggered)

| Property | Value |
|----------|-------|
| **Responsibility** | Receive firmware image via BLE, write to OTA partition, validate, swap boot partition |
| **Location** | `src/services/ota/ota_service.cpp`, `include/services/ota/ota_service.h` |
| **Interface** | `include/interfaces/i_ota.h` |
| **Communicates With** | BleService (receives firmware data), ScreenManager (shows OTA progress via event queue), esp_ota_ops (ESP-IDF OTA API) |
| **Thread Affinity** | OTA chunk buffering in NimBLE context, flash writes deferred to system_task or brief dedicated flush |
| **Owns** | OTA partition handle, write progress, image validation, state machine |

**OTA State Machine:**

```
IDLE -> PREPARING -> RECEIVING -> VERIFYING -> REBOOTING
  ^        |            |            |
  |        v            v            v
  +--- ABORTING <-- ABORTING <-- FAILED
```

**BLE MTU and chunk size:**

- Default BLE MTU: 23 bytes (too small for OTA)
- Request MTU negotiation to 512 bytes (NimBLE supports this)
- OTA chunk size: ~500 bytes per write (after MTU overhead)
- For a 1.5MB firmware: ~3000 chunks at 500 bytes = ~30-60 seconds over BLE 5.0
- Use "write without response" for OTA data characteristic (faster, no ACK overhead)

**OTA safety:**

- Firmware validated with SHA-256 before committing
- `esp_ota_mark_app_valid_cancel_rollback()` called AFTER successful self-test on new firmware boot (BLE init, LVGL init, touch response)
- If new firmware crashes before self-test, watchdog triggers reboot and bootloader automatically rolls back to previous partition
- Requires `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` in sdkconfig

### 5. Flash Partition Layout (OTA-Enabled)

**Current partition table (NO OTA):**
```
Name      Type  SubType  Offset    Size      Notes
nvs       data  nvs      0x9000    0x5000    (20KB)
phy_init  data  phy      0xe000    0x1000    (4KB)
app0      app   factory  0x10000   0x300000  (3MB)
spiffs    data  spiffs   0x310000  0x100000  (1MB)
                                    --------
                         Used:     ~4.1MB of 16MB
                         Free:     ~11.9MB
```

**New partition table (with OTA + NVS data):**
```
Name      Type  SubType  Offset    Size      Notes
nvs       data  nvs      0x9000    0x5000    (20KB - system NVS)
otadata   data  ota      0xe000    0x2000    (8KB - OTA boot tracking)
phy_init  data  phy      0x10000   0x1000    (4KB)
app0      app   ota_0    0x20000   0x300000  (3MB - primary firmware)
app1      app   ota_1    0x320000  0x300000  (3MB - OTA firmware)
spiffs    data  spiffs   0x620000  0x100000  (1MB - LittleFS audio/images)
nvs_data  data  nvs      0x720000  0x10000   (64KB - journey persistence)
                                    --------
                         Used:     ~7.2MB of 16MB
                         Free:     ~8.8MB (ample headroom)
```

**Key changes:**
- `otadata` partition added (8KB) -- tracks which app partition is active
- `app0` changes from `factory` to `ota_0`
- `app1` added as `ota_1` (same 3MB size)
- `nvs_data` added for journey data persistence (separate from system NVS to avoid BLE bond conflicts)
- Total: ~7.2MB, well within 16MB flash

**BREAKING CHANGE WARNING:** Changing the partition table requires a full serial reflash of every device. After this one-time reflash, all future updates can use OTA via BLE. Plan this transition before deploying to the field.

### 6. NVS Persistence Service

| Property | Value |
|----------|-------|
| **Responsibility** | Persist journey data, configuration, and user settings across reboots |
| **Location** | `src/services/nvs/nvs_manager.cpp`, `include/services/nvs/nvs_manager.h` |
| **Interface** | `include/interfaces/i_storage.h` |
| **Communicates With** | JornadaService (save/restore state), BleService (bonding via NimBLE's own NVS), SettingsScreen (user preferences), AudioManager (volume) |

NimBLE handles its own bonding key storage in NVS internally (when `CONFIG_BT_NIMBLE_NVS_PERSIST=y`). NvsManager handles application-level persistence.

```cpp
class NvsManager {
public:
    // Settings with defaults
    uint8_t getVolume();          // default: AUDIO_VOLUME_DEFAULT
    void setVolume(uint8_t v);
    uint8_t getBrightness();      // default: 100
    void setBrightness(uint8_t b);

    // Journey state persistence
    bool saveJornadaState(int motoristId, const DadosMotorista& dados);
    bool loadJornadaState(int motoristId, DadosMotorista* dados);
    void clearJornadaState(int motoristId);

    // All operations use separate NVS namespace "app_data" on nvs_data partition
};
```

### 7. Existing Services (Unchanged API)

| Service | Interface Changes | Internal Changes |
|---------|-------------------|------------------|
| IgnicaoService | None | None. BleService reads via existing getStatus()/getStats() |
| JornadaService | None | Add NVS persistence hooks: save on state change, restore on boot |
| AudioManager | None | Add setVolume() persistence via NvsManager. BleService writes via existing setVolume() |

## Data Flow Diagrams

### Journey State Change (device to app)

```
User Touch -> LVGL event (Core 0)
  -> JornadaScreen::onButtonClicked()
  -> JornadaService::iniciarEstado(driverId, state)
  -> JornadaService saves to NVS
  -> JornadaService fires JornadaCallback
  -> system_task posts event to appEventQueue
  -> BleService::notifyJornadaState() sends GATT notification
  -> Mobile app receives notification
```

### Configuration Change (app to device)

```
App writes GATT characteristic -> NimBLE GATT write callback (NimBLE task)
  -> BleService validates payload
  -> BleService posts AppEventData{CONFIG_CHANGED} to appEventQueue
  -> system_task dequeues event (Core 0)
  -> NvsManager.setConfig(key, value)
  -> SettingsScreen.refresh() (if active screen)
  -> AudioManager.setVolume() / backlight adjustment
```

### OTA Data Flow

```
Mobile App                  BLE Service              OTA Service
    |                           |                        |
    |-- Write OTA Control ----->|                        |
    |   (START, fw_size, sha)   |-- startOTA() --------->|
    |                           |                        |-- esp_ota_begin()
    |                           |<-- OTA_READY ----------|
    |<-- Notify OTA Status -----|                        |
    |                           |                        |
    |-- Write OTA Data -------->|                        |
    |   (chunk N, ~500 bytes)   |-- writeChunk() ------->|
    |   (write-no-response)     |                        |-- buffer in RAM
    |                           |                        |-- flush to flash (4KB)
    |   ... repeat per chunk... |                        |
    |                           |                        |
    |-- Write OTA Control ----->|                        |
    |   (FINISH)                |-- finishOTA() -------->|
    |                           |                        |-- esp_ota_end()
    |                           |                        |-- SHA-256 verify
    |                           |                        |-- esp_ota_set_boot_partition()
    |                           |<-- OTA_COMPLETE -------|
    |<-- Notify OTA Status -----|                        |
    |                           |                        |-- esp_restart()
```

### Screen Navigation Flow

```
User Touch -> LVGL event -> StatusBar menu button callback
  -> ScreenManager::navigateTo(SETTINGS)
  -> ScreenManager pushes current to stack
  -> Current screen: onExit()
  -> Settings screen: create() if not created, onEnter()
  -> lv_scr_load_anim() with slide transition

User Touch -> LVGL event -> Back button callback
  -> ScreenManager::goBack()
  -> ScreenManager pops stack
  -> Current screen: onExit()
  -> Previous screen: onEnter()
  -> lv_scr_load_anim() with reverse slide
```

## Patterns to Follow

### Pattern 1: Service Interface with C/C++ Dual API

**What:** Every new service implements a C++ abstract interface AND provides extern "C" functions.
**When:** Always. This is the established codebase convention.
**Why:** The codebase mixes C and C++. Extern "C" ensures C HAL code can interact with services.

```cpp
// include/interfaces/i_ble.h
class IBleService {
public:
    virtual ~IBleService() = default;
    virtual bool init() = 0;
    virtual bool isConnected() const = 0;
    virtual void startAdvertising() = 0;
    virtual void stopAdvertising() = 0;
    virtual void disconnect() = 0;
    virtual void notifyJornadaState(uint8_t driverId, uint8_t state) = 0;
    virtual void notifyIgnicaoState(bool on, uint32_t durationMs) = 0;
};

extern "C" {
    bool ble_init(void);
    bool ble_is_connected(void);
    void ble_start_advertising(void);
}
```

### Pattern 2: FreeRTOS Queue for Cross-Task Communication

**What:** Use FreeRTOS queues to pass events between tasks.
**When:** Any time BLE needs to trigger UI changes, or any cross-task communication.
**Why:** LVGL is single-threaded. NimBLE callbacks run in a different task context. Direct function calls between tasks cause race conditions.

See Section 3 (BLE-to-UI Communication) above for detailed example.

### Pattern 3: Screen as Self-Contained Unit

**What:** Each screen owns its LVGL objects, creates them in `create()`, destroys them in `destroy()`, manages its own state.
**When:** Every screen in the screen manager.
**Why:** Prevents button_manager.cpp monolith pattern (1205 lines). Each screen is 200-400 lines.

```cpp
class SettingsScreen : public IScreen {
private:
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* volumeSlider_ = nullptr;

public:
    ScreenType getType() const override { return ScreenType::SETTINGS; }

    void create() override {
        screen_ = lv_obj_create(NULL);  // detached screen
        volumeSlider_ = lv_slider_create(screen_);
        // ... configure all widgets ...
    }

    void onEnter() override {
        if (!screen_) create();
        lv_scr_load_anim(screen_, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        // false = don't auto-delete old screen (ScreenManager manages lifecycle)
    }

    void onExit() override {
        // Screen stays in memory for fast re-entry (destroy only if memory-constrained)
    }

    void destroy() override {
        if (screen_) { lv_obj_del(screen_); screen_ = nullptr; }
    }

    bool isCreated() const override { return screen_ != nullptr; }
    void update() override { /* periodic updates */ }
};
```

### Pattern 4: Singleton Service with Lazy Init

**What:** Services use static instance pointer with `getInstance()` / `destroyInstance()`. Init is explicit.
**When:** All new services (BleService, OtaService, NvsManager).
**Why:** Consistency with existing IgnicaoService and JornadaService patterns.

### Pattern 5: OTA State Machine

**What:** OTA process modeled as explicit state machine to handle errors, interruptions, and rollback.
**When:** OTA service implementation.

```
IDLE -> PREPARING -> RECEIVING -> VERIFYING -> REBOOTING
  ^        |            |            |
  |        v            v            v
  +--- ABORTING <-- ABORTING <-- FAILED
```

### Pattern 6: NVS Typed Wrapper

**What:** Wrap raw NVS API in typed C++ interface to prevent key typos and type mismatches.
**When:** Any NVS access.

```cpp
class NvsManager {
public:
    uint8_t getVolume() { return getU8("volume", AUDIO_VOLUME_DEFAULT); }
    void setVolume(uint8_t v) { setU8("volume", v); }
    // BLE bond storage delegated to NimBLE's internal NVS usage
private:
    uint8_t getU8(const char* key, uint8_t def);
    void setU8(const char* key, uint8_t val);
};
```

### Pattern 7: Centralized Configuration

**What:** All new constants go in `app_config.h` (or a dedicated `ble_config.h` for BLE UUIDs).
**When:** Always. Never hardcode values in implementation files.

```c
// app_config.h additions:

// BLE
#define BLE_DEVICE_NAME         "TecladoJornada"
#define BLE_ADV_INTERVAL_MIN    160   // 100ms in 0.625ms units
#define BLE_ADV_INTERVAL_MAX    800   // 500ms
#define BLE_CONN_INTERVAL_MIN   24    // 30ms
#define BLE_CONN_INTERVAL_MAX   40    // 50ms
#define BLE_MTU_PREFERRED       512
#define BLE_TASK_STACK_SIZE     (6 * 1024)
#define BLE_TASK_PRIORITY       4

// OTA
#define OTA_CHUNK_SIZE          512
#define OTA_BUFFER_SIZE         (4 * 1024)  // 4KB RAM buffer before flash write
#define OTA_TIMEOUT_MS          120000      // 2 min total timeout
#define OTA_SELF_TEST_TIMEOUT   60000       // 60s for post-OTA self-test

// NVS
#define NVS_NAMESPACE_APP       "app_data"
#define NVS_PARTITION_LABEL     "nvs_data"
```

## Anti-Patterns to Avoid

### Anti-Pattern 1: Direct LVGL Calls from BLE Callbacks

**What:** Calling `lv_label_set_text()` or any `lv_*` function from a NimBLE callback.
**Why bad:** NimBLE callbacks run on the NimBLE Host Task, not system_task. LVGL is not thread-safe. Causes memory corruption, display glitches, or crashes.
**Instead:** Post event to `appEventQueue`; handle LVGL updates in system_task.

### Anti-Pattern 2: Blocking Flash Writes in NimBLE Context

**What:** Calling `esp_ota_write()` directly in a BLE GATT write callback.
**Why bad:** Flash writes take 10-40ms. Blocking NimBLE task causes BLE connection timeouts and dropped packets.
**Instead:** Buffer OTA chunks in a RAM buffer (4KB); flush to flash from system_task or a dedicated brief flush when buffer is full.

### Anti-Pattern 3: Monolithic Screen Class

**What:** One giant class handling all screens (current button_manager.cpp at 1205 lines).
**Why bad:** Violates single responsibility, makes adding new screens painful, impossible to maintain.
**Instead:** One class per screen implementing IScreen. ScreenManager only handles navigation. Each screen file is 200-400 lines.

### Anti-Pattern 4: Skipping Protocol Version Negotiation

**What:** Not including protocol version in BLE handshake.
**Why bad:** When app and firmware evolve independently, protocol mismatches cause silent failures.
**Instead:** Include protocol version in Device Info characteristic. App checks version on connect.

### Anti-Pattern 5: OTA Without Rollback Verification

**What:** Marking OTA partition as valid immediately after boot.
**Why bad:** If new firmware has a runtime bug (crashes after 30 seconds), device is bricked.
**Instead:** Call `esp_ota_mark_app_valid_cancel_rollback()` ONLY after successful self-test (BLE init OK, LVGL init OK, touch responsive). Use watchdog timer -- if self-test doesn't complete within 60 seconds, reboot triggers automatic rollback.

### Anti-Pattern 6: Using Bluedroid Instead of NimBLE

**What:** Using ESP-IDF's Bluedroid BLE stack.
**Why bad:** Bluedroid consumes ~100KB more RAM than NimBLE, designed for Classic BT + BLE combo, more complex for BLE-only.
**Instead:** NimBLE. ~50KB RAM, designed for BLE-only, recommended for ESP32-S3 BLE-only applications.

## Task Priority Summary

| Task | Core | Priority | Stack | Purpose |
|------|------|----------|-------|---------|
| BLE Controller | 0 | 22 | ~4KB | HCI layer (auto-created by ESP-IDF, must be high for timing) |
| system_task | 0 | 5 | 8KB | LVGL event loop, screen updates, BLE event processing |
| audio_task | 1 | 5 | 32KB | MP3 decode + I2S write (only task on Core 1) |
| NimBLE Host | 0 | 4 | 4-6KB | BLE event processing (auto-created by nimble_port) |
| IgnicaoMonitor | 0 | 2 | 4KB | GPIO18 polling with debounce every 100ms |

**Priority rationale:**
- BLE Controller at 22: ESP-IDF default, must be high for BLE timing compliance
- system_task at 5: UI responsiveness is user-facing
- NimBLE Host at 4: Below UI (BLE events can wait one LVGL tick), above ignition
- IgnicaoMonitor at 2: Low-frequency polling (100ms interval), not time-critical
- audio_task at 5 on Core 1: Only task on that core, priority doesn't compete

## Scalability Considerations

| Concern | Current (v1.x) | With BLE+OTA (v2.0) | Future |
|---------|-----------------|---------------------|--------|
| RAM usage | ~100KB internal + PSRAM for display | +50KB NimBLE + 4KB OTA buffer | Monitor with heap_caps_get_free_size() |
| Flash usage | ~1.5MB app + 1MB LittleFS | ~1.5MB x2 (OTA) + 1MB LFS + 64KB NVS = ~7.2MB | 8.8MB free headroom |
| BLE connections | N/A | 1 concurrent (point-to-point) | Could support 2-3 if needed |
| Screens | 2 (numpad + jornada via swipe) | 5+ with stack navigation | Add screens without touching existing ones |
| Services | 3 (ignicao, jornada, audio) | 6 (+ BLE, OTA, NVS) | Service registry pattern if >10 |
| OTA deployment | N/A | Manual per device via mobile app | Fleet management needs server |

## Suggested Build Order (Dependencies)

```
Phase 1: Screen Manager (no external dependencies)
    |
    +-- Depends on: existing i_screen.h interface (already exists)
    +-- Blocked by: nothing
    +-- Enables: all new screens, menu navigation, OTA screen
    +-- Includes: StatusBar menu button, basic navigation
    |
Phase 2: NVS Persistence (no external dependencies, CAN PARALLEL with Phase 1)
    |
    +-- Depends on: ESP-IDF NVS API (already available)
    +-- Blocked by: nothing
    +-- Enables: journey persistence, settings persistence, BLE bonding
    |
Phase 3: BLE Service (depends on NVS for bonding)
    |
    +-- Depends on: NVS (Phase 2) for bonding persistence
    +-- Blocked by: Phase 2
    +-- Enables: mobile app communication, OTA data channel
    +-- Requires: sdkconfig changes (BT_ENABLED, NIMBLE)
    |
Phase 4: OTA Service (depends on BLE + Screen Manager)
    |
    +-- Depends on: BLE (Phase 3) for data channel, Screen Manager (Phase 1) for OTA screen
    +-- Blocked by: Phase 1 + Phase 3
    +-- Requires: partition table change (BREAKING -- one-time serial reflash)
    |
Phase 5: button_manager.cpp Decomposition (depends on Screen Manager)
    |
    +-- Depends on: Screen Manager (Phase 1)
    +-- Can run in parallel with Phases 3-4
    +-- Extracts JornadaScreen, NumpadScreen from monolithic file
```

**Critical path:** Phase 2 -> Phase 3 -> Phase 4 (NVS -> BLE -> OTA)

**Parallel opportunities:**
- Phase 1 (Screen Manager) and Phase 2 (NVS) start simultaneously
- Phase 5 (decomposition) starts after Phase 1, runs parallel with BLE/OTA work

## sdkconfig Changes Required

Add to `sdkconfig.defaults`:

```ini
# ============================================================================
# BLE (NimBLE)
# ============================================================================
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
CONFIG_BT_NIMBLE_ROLE_CENTRAL=n
CONFIG_BT_NIMBLE_ROLE_OBSERVER=n
CONFIG_BT_NIMBLE_ROLE_BROADCASTER=y
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_BT_NIMBLE_NVS_PERSIST=y
CONFIG_BT_NIMBLE_SM_LEGACY=n
CONFIG_BT_NIMBLE_SM_SC=y
CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT=y

# ============================================================================
# OTA
# ============================================================================
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

## New File Structure (Proposed)

```
include/
  interfaces/
    i_ble.h              (NEW)
    i_ota.h              (NEW)
    i_storage.h          (NEW - NVS manager interface)
    i_screen.h           (MODIFY - add new ScreenType values)
    i_ignicao.h          (EXISTING)
    i_jornada.h          (EXISTING)
    i_audio.h            (EXISTING)
  config/
    app_config.h         (MODIFY - add BLE, OTA, NVS constants)
    ble_uuids.h          (NEW - 128-bit GATT service/char UUIDs)
  services/
    ble/
      ble_service.h      (NEW)
    ota/
      ota_service.h      (NEW)
    nvs/
      nvs_manager.h      (NEW)
  ui/
    screens/
      screen_manager.h   (NEW)

src/
  services/
    ble/
      ble_service.cpp         (NEW - NimBLE init, GAP, security)
      ble_gatt_journey.cpp    (NEW - Journey GATT service)
      ble_gatt_config.cpp     (NEW - Config GATT service)
      ble_gatt_ota.cpp        (NEW - OTA GATT service)
    ota/
      ota_service.cpp         (NEW - esp_ota_ops wrapper, state machine)
    nvs/
      nvs_manager.cpp         (NEW - typed NVS wrapper)
  ui/
    screens/
      screen_manager.cpp      (NEW - IScreenManager implementation)
      jornada_screen.cpp      (NEW - refactored from button_manager.cpp)
      settings_screen.cpp     (NEW - volume, brightness, BLE toggle)
      ota_screen.cpp          (NEW - OTA progress display)
      numpad_screen.cpp       (NEW - refactored from numpad_example)
      rpm_screen.cpp          (NEW - RPM/velocity display)
    widgets/
      status_bar.cpp          (MODIFY - add BLE icon, menu button)
```

## Sources

- Existing codebase analysis: `src/main.cpp`, `include/interfaces/i_*.h`, `include/config/app_config.h`, `partitions.csv`, `sdkconfig.defaults`, `sdkconfig.esp32s3_idf`
- Existing architecture documentation: `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/CONCERNS.md`, `.planning/codebase/STACK.md`, `.planning/codebase/STRUCTURE.md`
- Project requirements: `.planning/PROJECT.md`
- ESP-IDF NimBLE documentation (training data, MEDIUM confidence)
- ESP-IDF OTA API `esp_ota_ops.h` (training data, MEDIUM confidence)
- ESP-IDF partition table format (training data, HIGH confidence -- stable API)
- LVGL 8.4 screen management with `lv_scr_load_anim()` (training data, MEDIUM confidence)
- FreeRTOS queue patterns (training data, HIGH confidence -- fundamental RTOS pattern)

### Confidence Assessment

| Area | Confidence | Rationale |
|------|------------|-----------|
| Screen Manager pattern | HIGH | Based on existing i_screen.h interface already in codebase + well-known LVGL pattern |
| NimBLE task model | MEDIUM | NimBLE host task runs on Core 0 by default; SOC capability flags confirmed in sdkconfig |
| NimBLE memory footprint | MEDIUM | ~50KB widely cited; exact number varies by NimBLE configuration |
| Partition table layout | HIGH | ESP-IDF partition format is stable; math verified against 16MB flash constraints |
| OTA via BLE flow | MEDIUM | Standard pattern; BLE MTU negotiation details should be verified against NimBLE docs |
| BLE-to-UI queue pattern | HIGH | FreeRTOS queue is canonical cross-task mechanism; LVGL single-thread requirement is absolute |
| Task priorities | MEDIUM | NimBLE controller pri 22 is ESP-IDF default; other priorities are design decisions |
| sdkconfig flags | MEDIUM | NimBLE config flags from training data; verify exact names against ESP-IDF 5.3.1 docs |
