# Phase 2: BLE Core - Research

**Researched:** 2026-02-10
**Domain:** NimBLE BLE stack, GATT server, LE Secure Connections, FreeRTOS event bridge
**Confidence:** HIGH

## Summary

Phase 2 integrates NimBLE as a BLE peripheral GATT server on the ESP32-S3, exposing journey states, ignition status, device info, and diagnostics over Bluetooth LE. The core NimBLE stack is an ESP-IDF built-in component -- no external libraries are needed. All NimBLE APIs, GATT service definitions, and security configuration are well-documented with official examples (`bleprph`, `blehr`, `NimBLE_GATT_Server`) that directly apply to this phase.

Key research findings: (1) The LVGL display buffers are already allocated from PSRAM (`buff_spiram = true` in `esp_bsp.c`), so the roadmap risk about "migrating LVGL buffers to PSRAM" is already resolved by the existing codebase. (2) NimBLE bond storage uses the default system `nvs` partition (namespace `"nimble_bond"`), NOT the custom `nvs_data` partition used by the application -- the system `nvs` partition must be initialized before NimBLE starts. (3) The Device Information Service (0x180A) has a built-in implementation in NimBLE (`ble_svc_dis`) that can be used directly. (4) NimBLE on ESP32-S3 with peripheral-only role and 1 connection uses approximately 50KB internal SRAM at runtime with optimized memory settings.

**Primary recommendation:** Follow the `bleprph` example pattern for NimBLE initialization, GAP advertising, and GATT service definition. Use a FreeRTOS queue (not direct callbacks) for all BLE-to-UI communication. Initialize the default `nvs` partition (`nvs_flash_init()`) in addition to the existing `nvs_data` partition before NimBLE starts.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| NimBLE (esp-nimble) | Bundled with ESP-IDF 5.3.1 | BLE 5.0 stack, GATT server | Built into ESP-IDF, 50% less RAM than Bluedroid, BLE-only, full security manager |
| esp_bt controller | ESP-IDF 5.3.1 | BLE radio controller | Required by NimBLE, manages HCI interface to radio |
| NVS Flash (esp_nvs) | ESP-IDF 5.3.1 | Bond persistence | NimBLE stores bonds in default `nvs` partition automatically |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| ble_svc_gap | NimBLE bundled | GAP service (device name) | Always -- required by BLE spec |
| ble_svc_gatt | NimBLE bundled | GATT service (service changed) | Always -- required by BLE spec |
| ble_svc_dis | NimBLE bundled | Device Information Service (0x180A) | For GATT-01 -- provides manufacturer, model, firmware, hardware revision |
| FreeRTOS queue | ESP-IDF 5.3.1 | BLE-to-UI event bridge | BLE-07 -- all BLE state changes that need UI updates |
| esp_timer | ESP-IDF 5.3.1 | Uptime tracking | GATT-08 diagnostics -- device uptime |
| esp_app_desc | ESP-IDF 5.3.1 | Firmware version | GATT-01, BLE-08 -- runtime version info |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| NimBLE | Bluedroid | 2x RAM, includes unused Classic BT -- wrong choice for BLE-only device |
| ble_svc_dis (built-in) | Custom DIS implementation | More work, no benefit -- built-in handles SIG-compliant 0x180A |
| FreeRTOS queue | esp_event loop | esp_event adds overhead and indirection; raw queue is simpler for fixed event types |
| Binary GATT values | JSON over BLE | JSON wastes bandwidth in MTU-constrained BLE; binary structs are 2-3x more efficient |

### No Installation Needed
NimBLE is an ESP-IDF component -- no `lib_deps` needed in `platformio.ini`. Enable via `sdkconfig.defaults` flags only.

## Architecture Patterns

### Recommended Project Structure
```
src/
  services/
    ble/
      ble_service.h           # IBleService interface
      ble_service.cpp         # NimBLE init, GAP, security manager
      ble_gap.cpp             # Advertising, connection, gap event handler
      ble_event_queue.h       # FreeRTOS queue for BLE-to-UI events
      ble_event_queue.cpp     # Event types, post, consume
    ble/gatt/
      gatt_server.h           # GATT service table and init
      gatt_server.cpp         # Service definitions, access callbacks
      gatt_journey.h          # Journey service data packing
      gatt_journey.cpp        # Journey characteristic read/notify
      gatt_diagnostics.h      # Diagnostics service
      gatt_diagnostics.cpp    # Heap, uptime, queue depth
include/
  interfaces/
    i_ble.h                   # Abstract BLE service interface
  config/
    ble_uuids.h               # All custom BLE UUIDs in one place
```

### Pattern 1: NimBLE Initialization Sequence
**What:** Standard init sequence from `bleprph` example
**When to use:** Once at startup, in `system_task` after NVS init

```cpp
// Source: ESP-IDF bleprph example
// 1. Init default NVS partition (for NimBLE bond storage)
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    ret = nvs_flash_init();
}

// 2. Init BT controller
esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
esp_bt_controller_init(&bt_cfg);
esp_bt_controller_enable(ESP_BT_MODE_BLE);

// 3. Init NimBLE port
nimble_port_init();

// 4. Configure host callbacks
ble_hs_cfg.reset_cb = on_ble_reset;
ble_hs_cfg.sync_cb = on_ble_sync;  // Start advertising here
ble_hs_cfg.gatts_register_cb = on_gatt_register;
ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

// 5. Configure security
ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;  // Just Works
ble_hs_cfg.sm_bonding = 1;
ble_hs_cfg.sm_sc = 1;  // LE Secure Connections
ble_hs_cfg.sm_mitm = 0;  // No MITM (Just Works)
ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

// 6. Init GATT services
ble_svc_gap_init();
ble_svc_gatt_init();
gatt_svr_init();  // Custom services

// 7. Set device name
ble_svc_gap_device_name_set("GS-Jornada-XXXX");

// 8. Init bond store
ble_store_config_init();

// 9. Set preferred MTU
ble_att_set_preferred_mtu(BLE_MTU_PREFERRED);

// 10. Start NimBLE host task
nimble_port_freertos_init(ble_host_task);
```

### Pattern 2: GATT Service Definition
**What:** Declarative service/characteristic table
**When to use:** For every GATT service

```cpp
// Source: ESP-IDF bleprph/gatt_svr.c + official docs

// Custom 128-bit UUIDs
static const ble_uuid128_t journey_svc_uuid =
    BLE_UUID128_INIT(0x01, 0x00, 0x00, 0x00, 0x65, 0x73, 0x63, 0x47,
                     0x00, 0x00, 0x4a, 0x47, 0x01, 0x00, 0x00, 0x00);

static const ble_uuid128_t journey_state_chr_uuid =
    BLE_UUID128_INIT(0x01, 0x01, 0x00, 0x00, 0x65, 0x73, 0x63, 0x47,
                     0x00, 0x00, 0x4a, 0x47, 0x01, 0x00, 0x00, 0x00);

// Value handle (populated after registration)
static uint16_t journey_state_val_handle;

// Service table
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &journey_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &journey_state_chr_uuid.u,
                .access_cb = journey_state_access,
                .val_handle = &journey_state_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 }  // Terminator
        },
    },
    { 0 }  // End of services
};

// Init function
int gatt_svr_init(void) {
    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) return rc;
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    return rc;
}
```

### Pattern 3: Characteristic Access Callback
**What:** Handle read/write requests from BLE clients
**When to use:** Every characteristic needs one

```cpp
// Source: ESP-IDF data exchange documentation
static int journey_state_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: {
        // Pack journey state data
        uint8_t data[16];
        pack_journey_state(data, sizeof(data));
        int rc = os_mbuf_append(ctxt->om, data, sizeof(data));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        // Validate write data
        if (ctxt->om->om_len < MIN_WRITE_LEN || ctxt->om->om_len > MAX_WRITE_LEN) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        // Process write...
        return 0;
    }
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}
```

### Pattern 4: Sending Notifications
**What:** Push data to connected client when state changes
**When to use:** Journey state changes, ignition changes

```cpp
// Source: ESP-IDF blehr example + ble_gatts API
static uint16_t conn_handle_current = 0;
static bool notify_enabled = false;

// Called from gap event handler
void on_subscribe(struct ble_gap_event *event) {
    if (event->subscribe.attr_handle == journey_state_val_handle) {
        notify_enabled = event->subscribe.cur_notify;
        conn_handle_current = event->subscribe.conn_handle;
    }
}

// Call when journey state changes
void notify_journey_state(void) {
    if (!notify_enabled) return;

    uint8_t data[16];
    pack_journey_state(data, sizeof(data));

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, sizeof(data));
    if (om) {
        ble_gatts_notify_custom(conn_handle_current, journey_state_val_handle, om);
    }
}
```

### Pattern 5: BLE-to-UI Event Queue (BLE-07)
**What:** FreeRTOS queue decouples NimBLE callbacks from LVGL updates
**When to use:** Every BLE state change that needs a UI update

```cpp
// Event types
enum class BleEventType : uint8_t {
    DISCONNECTED = 0,
    ADVERTISING,
    CONNECTED,
    SECURED,
    MTU_CHANGED,
};

struct BleEvent {
    BleEventType type;
    uint16_t conn_handle;
    uint16_t mtu;
};

// Queue (created once at init)
static QueueHandle_t ble_event_queue = nullptr;

// Producer (called from NimBLE GAP callback -- any core)
void ble_post_event(BleEventType type, uint16_t conn_handle = 0) {
    BleEvent evt = { .type = type, .conn_handle = conn_handle };
    xQueueSend(ble_event_queue, &evt, 0);  // Non-blocking
}

// Consumer (called from system_task on Core 0 -- LVGL safe)
void ble_process_events(StatusBar& statusBar) {
    BleEvent evt;
    while (xQueueReceive(ble_event_queue, &evt, 0) == pdTRUE) {
        switch (evt.type) {
            case BleEventType::DISCONNECTED:
                statusBar.setBleStatus(BLE_STATUS_DISCONNECTED);
                break;
            case BleEventType::CONNECTED:
                statusBar.setBleStatus(BLE_STATUS_CONNECTED);
                break;
            // ... etc
        }
    }
}
```

### Pattern 6: GAP Advertising with Dynamic Name (BLE-02)
**What:** Device name includes last 4 hex digits of MAC address
**When to use:** During advertising setup in `on_ble_sync()` callback

```cpp
// Get MAC and format device name
uint8_t mac[6];
esp_read_mac(mac, ESP_MAC_BT);
char device_name[32];
snprintf(device_name, sizeof(device_name), "%s-%02X%02X",
         BLE_DEVICE_NAME_PREFIX, mac[4], mac[5]);
ble_svc_gap_device_name_set(device_name);

// Set advertising data
struct ble_hs_adv_fields fields = {};
fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
fields.name = (uint8_t*)device_name;
fields.name_len = strlen(device_name);
fields.name_is_complete = 1;
fields.tx_pwr_lvl_is_present = 1;
fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

// Include service UUIDs in scan response (if too long for adv data)
ble_gap_adv_set_fields(&fields);

// Start advertising
struct ble_gap_adv_params adv_params = {};
adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                  &adv_params, gap_event_handler, NULL);
```

### Anti-Patterns to Avoid
- **Calling LVGL from BLE callbacks:** BLE GAP/GATT callbacks run in the NimBLE host task. LVGL is NOT thread-safe. Always post to the event queue and let `system_task` handle UI updates.
- **Blocking in BLE callbacks:** NimBLE callbacks must return quickly. Do not call `vTaskDelay`, mutex waits, or NVS writes from inside a callback. Post work to a queue.
- **Sending notifications too fast:** `ble_gatts_notify_custom()` can return ENOMEM if called faster than BLE can transmit. Always check the return value and use rate limiting (100ms minimum between notifications is safe).
- **Forgetting to init default NVS:** NimBLE bond storage uses `nvs_open()` (default partition), not `nvs_open_from_partition()`. The default `nvs` partition must be initialized with `nvs_flash_init()` before `nimble_port_init()`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Device Information Service | Custom GATT service for 0x180A | `ble_svc_dis` (NimBLE built-in) | SIG-compliant, handles all standard characteristics, zero code |
| GAP service | Manual name/appearance characteristics | `ble_svc_gap` (NimBLE built-in) | Required by BLE spec, handles device name automatically |
| Bond key storage | Custom NVS store for encryption keys | `ble_store_config_init()` + `CONFIG_BT_NIMBLE_NVS_PERSIST` | NimBLE handles all key lifecycle, storage, and retrieval automatically |
| GATT CCCD management | Manual Client Characteristic Config Descriptors | NimBLE auto-adds CCCD when `BLE_GATT_CHR_F_NOTIFY` flag is set | NimBLE tracks subscription state per-connection automatically |
| BLE address type management | Manual address resolution | `ble_hs_id_infer_auto(0, &own_addr_type)` | Handles public/random/RPA address selection automatically |
| MTU negotiation protocol | Custom MTU exchange | `ble_att_set_preferred_mtu()` + automatic exchange on connect | BLE spec requires client to initiate; server just declares preference |

**Key insight:** NimBLE handles the entire BLE protocol stack including security, bonding, CCCD tracking, and MTU negotiation. The application only defines the GATT service table, implements access callbacks for read/write, and sends notifications when data changes.

## Common Pitfalls

### Pitfall 1: Default NVS Partition Not Initialized
**What goes wrong:** NimBLE bond persistence silently fails; device re-pairs on every power cycle
**Why it happens:** The project only initializes `nvs_data` partition via `nvs_flash_init_partition("nvs_data")`. NimBLE uses `nvs_open()` which accesses the default `nvs` partition (at 0x9000 in the partition table). If `nvs_flash_init()` is never called, bond writes succeed but data is not persisted.
**How to avoid:** Call `nvs_flash_init()` (no partition argument) BEFORE `nimble_port_init()`. This is separate from the existing `NvsManager::init()` which initializes `nvs_data`.
**Warning signs:** Pairing works but bonds are lost after reboot; NVS error logs mentioning "nimble_bond"

### Pitfall 2: LVGL Calls from NimBLE Callbacks
**What goes wrong:** Crash, data corruption, or visual glitches
**Why it happens:** NimBLE GAP/GATT callbacks execute in the NimBLE host task (Core 0, priority 4 or lower). LVGL is not thread-safe and must only be called from `system_task` context.
**How to avoid:** Use FreeRTOS queue (BLE-07 pattern). BLE callbacks post events; `system_task` loop consumes them and updates UI.
**Warning signs:** Random crashes during BLE connect/disconnect, garbled status bar

### Pitfall 3: Notification Rate Overflow
**What goes wrong:** `ble_gatts_notify_custom()` returns `BLE_HS_ENOMEM` (12)
**Why it happens:** Notifications are queued in NimBLE's MSYS mbuf pool. If the application sends faster than the BLE link can transmit, the pool exhausts.
**How to avoid:** Rate-limit notifications to max 10/second. Check return value. For time-in-state updates, use a 1-second timer, not per-millisecond updates.
**Warning signs:** ENOMEM errors in log, dropped notifications

### Pitfall 4: Audio Stuttering During BLE Activity
**What goes wrong:** MP3 playback has gaps or distortion during BLE connection events
**Why it happens:** NimBLE host task on Core 0 has bursts of activity during connection/pairing. If it preempts `system_task` which feeds LVGL (also Core 0), and the system is under load, Core 1 audio task may starve on shared resources.
**How to avoid:** Set NimBLE task priority BELOW `system_task` (NimBLE default priority = 4, system_task = 5 -- this is already correct). Audio is on Core 1 and should be unaffected, but verify with testing.
**Warning signs:** Audio clicks during pairing, dropped I2S frames

### Pitfall 5: Advertising Data Too Long (31 bytes limit)
**What goes wrong:** `ble_gap_adv_set_fields()` returns error
**Why it happens:** BLE advertising packet has a 31-byte payload limit. Device name ("GS-Jornada-XXXX" = 15 bytes) + flags (3 bytes) + TX power (3 bytes) + 128-bit UUID (18 bytes) = 39 bytes > 31.
**How to avoid:** Put service UUIDs in the Scan Response data (separate 31-byte packet) using `ble_gap_adv_rsp_set_fields()`. Keep only name + flags in the advertising data.
**Warning signs:** `BLE_HS_EMSGSIZE` error from advertising setup

### Pitfall 6: MTU Negotiation Expectations
**What goes wrong:** Developer expects 512-byte MTU but gets 23 bytes
**Why it happens:** MTU negotiation is initiated by the CLIENT, not the server. The server can only declare its preferred MTU via `ble_att_set_preferred_mtu()`. The actual MTU is `min(server_preferred, client_preferred)`. Many mobile BLE stacks default to 23 bytes until the app explicitly requests higher.
**How to avoid:** Set preferred MTU to 512 in sdkconfig (`CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=512`). Also set `ble_att_set_preferred_mtu(512)` at runtime. In `BLE_GAP_EVENT_MTU`, call `ble_hs_hci_util_set_data_len()` to maximize link-layer packet size. Accept that actual MTU depends on the client.
**Warning signs:** Low throughput in nRF Connect despite high MTU setting

## Code Examples

### Device Information Service Setup (GATT-01)
```cpp
// Source: NimBLE ble_svc_dis documentation
#include "services/dis/ble_svc_dis.h"

// In GATT init function:
void gatt_init(void) {
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Configure DIS fields
    ble_svc_dis_manufacturer_name_set(APP_COMPANY);    // "Getscale Sistemas Embarcados"
    ble_svc_dis_model_number_set("GS-Jornada");
    ble_svc_dis_firmware_revision_set(APP_VERSION_STRING);  // "1.0.0"
    ble_svc_dis_hardware_revision_set("ESP32-S3-R8");

    // Init DIS service
    ble_svc_dis_init();

    // Then add custom services...
    ble_gatts_count_cfg(custom_gatt_svcs);
    ble_gatts_add_svcs(custom_gatt_svcs);
}
```

### Journey State Characteristic Data Format (GATT-02, GATT-03)
```cpp
// Binary format for journey state per motorist
// Total: 3 motorists * 8 bytes = 24 bytes
struct __attribute__((packed)) JourneyStateData {
    uint8_t motorist_id;      // 0-2
    uint8_t state;            // EstadoJornada enum (0-6)
    uint8_t active;           // 0 or 1
    uint8_t reserved;         // Alignment
    uint32_t time_in_state;   // Milliseconds in current state
};

// Pack all 3 motorists
int pack_journey_states(uint8_t* buf, size_t buf_len) {
    if (buf_len < sizeof(JourneyStateData) * MAX_MOTORISTAS) {
        return -1;
    }
    auto* jornadaSvc = JornadaService::getInstance();
    JourneyStateData* out = reinterpret_cast<JourneyStateData*>(buf);
    for (int i = 0; i < MAX_MOTORISTAS; i++) {
        DadosMotorista dados;
        if (jornadaSvc->getMotorista(i + 1, &dados)) {
            out[i].motorist_id = i;
            out[i].state = static_cast<uint8_t>(dados.estadoAtual);
            out[i].active = dados.ativo ? 1 : 0;
            out[i].time_in_state = jornadaSvc->getTempoEstadoAtual(i + 1);
        } else {
            memset(&out[i], 0, sizeof(JourneyStateData));
            out[i].motorist_id = i;
        }
    }
    return sizeof(JourneyStateData) * MAX_MOTORISTAS;
}
```

### Ignition Characteristic Data Format (GATT-04)
```cpp
// Binary format for ignition status
struct __attribute__((packed)) IgnitionData {
    uint8_t status;           // 0=OFF, 1=ON
    uint8_t reserved[3];      // Alignment
    uint32_t duration_ms;     // Time in current state
};
```

### Diagnostics Characteristic Data Format (GATT-08)
```cpp
// Binary format for diagnostics
struct __attribute__((packed)) DiagnosticsData {
    uint32_t free_heap;           // Internal free heap bytes
    uint32_t min_free_heap;       // Minimum free heap since boot
    uint32_t psram_free;          // PSRAM free bytes
    uint32_t uptime_seconds;      // Seconds since boot
    uint16_t ble_connections;     // Active BLE connections (0 or 1)
    uint8_t  audio_queue_depth;   // Items in audio queue
    uint8_t  reserved;
};

void pack_diagnostics(DiagnosticsData* diag) {
    diag->free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    diag->min_free_heap = esp_get_minimum_free_heap_size();
    diag->psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    diag->uptime_seconds = (uint32_t)(esp_timer_get_time() / 1000000);
    diag->ble_connections = /* track in gap event handler */ 0;
    diag->audio_queue_depth = /* from audio manager */ 0;
}
```

### GAP Event Handler (Connection Lifecycle)
```cpp
// Source: ESP-IDF bleprph example
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            conn_handle_current = event->connect.conn_handle;
            ble_post_event(BleEventType::CONNECTED, conn_handle_current);
            // Initiate security
            ble_gap_security_initiate(conn_handle_current);
        } else {
            // Connection failed, restart advertising
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        conn_handle_current = 0;
        notify_enabled = false;
        ble_post_event(BleEventType::DISCONNECTED);
        // Restart advertising
        start_advertising();
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ble_post_event(BleEventType::SECURED, event->enc_change.conn_handle);
        }
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        handle_subscribe_event(event);
        break;

    case BLE_GAP_EVENT_MTU:
        ble_post_event(BleEventType::MTU_CHANGED);
        // Also set LL data length for throughput
        ble_hs_hci_util_set_data_len(event->mtu.conn_handle, 251, 2120);
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        // Delete old bond and accept new pairing
        struct ble_gap_conn_desc desc;
        ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    }
    return 0;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Bluedroid for all BLE | NimBLE (BLE-only) | ESP-IDF 4.0+ | 50% RAM savings, faster startup, simpler API |
| Manual bond storage | `CONFIG_BT_NIMBLE_NVS_PERSIST=y` | ESP-IDF 4.1+ | Automatic bond persistence, zero custom code |
| Manual DIS implementation | `ble_svc_dis` built-in | ESP-IDF 4.3+ (PR #6040) | SIG-compliant Device Info Service with one function call |
| `ble_gatts_indicate()` only | `ble_gatts_notify_custom()` with mbuf | Available since NimBLE 1.3 | Allows sending arbitrary data in notifications |

**Deprecated/outdated:**
- `esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)`: Not needed when using NimBLE (BLE-only controller mode)
- Manual CCCD handling: NimBLE auto-manages CCCDs when notify/indicate flags are set

## Required sdkconfig.defaults Additions

```ini
# ============================================================================
# BLE / NimBLE
# ============================================================================
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y

# Roles (peripheral only)
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_BT_NIMBLE_ROLE_BROADCASTER=y
CONFIG_BT_NIMBLE_ROLE_CENTRAL=n
CONFIG_BT_NIMBLE_ROLE_OBSERVER=n

# Connections
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1

# Security: LE Secure Connections
CONFIG_BT_NIMBLE_SM_LEGACY=n
CONFIG_BT_NIMBLE_SM_SC=y

# Bonding
CONFIG_BT_NIMBLE_NVS_PERSIST=y
CONFIG_BT_NIMBLE_MAX_BONDS=3

# Core affinity
CONFIG_BT_NIMBLE_PINNED_TO_CORE_0=y
CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=4096

# MTU
CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=512

# Memory optimization (peripheral-only, 1 connection)
CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT=12
CONFIG_BT_NIMBLE_MSYS_1_BLOCK_SIZE=256
CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT=24
CONFIG_BT_NIMBLE_MSYS_2_BLOCK_SIZE=320

# Logging
CONFIG_BT_NIMBLE_LOG_LEVEL_WARNING=y

# BLE 5.0 extended features (not needed)
CONFIG_BT_NIMBLE_EXT_ADV=n
CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT=n
```

## GATT Service Design (Complete)

### Service 1: Device Information Service (SIG 0x180A) -- GATT-01, BLE-08
Use built-in `ble_svc_dis`. Standard SIG 16-bit UUIDs.

| Characteristic | UUID | Properties | Data |
|----------------|------|------------|------|
| Manufacturer Name | 0x2A29 | Read | "Getscale Sistemas Embarcados" |
| Model Number | 0x2A24 | Read | "GS-Jornada" |
| Firmware Revision | 0x2A26 | Read | APP_VERSION_STRING ("1.0.0") |
| Hardware Revision | 0x2A27 | Read | "ESP32-S3-R8" |
| Software Revision | 0x2A28 | Read | Protocol version ("BLE-P1.0") -- for BLE-08 |

### Service 2: Journey Service (Custom UUID) -- GATT-02, GATT-03, GATT-04
Custom 128-bit base UUID format: `0000XXXX-4a47-0000-4763-7365-00000001`

| Characteristic | Short ID | Properties | Data Format | Size |
|----------------|----------|------------|-------------|------|
| Journey States | 0x0101 | Read, Notify | 3x JourneyStateData | 24 bytes |
| Ignition Status | 0x0102 | Read, Notify | IgnitionData | 8 bytes |

### Service 3: Diagnostics Service (Custom UUID) -- GATT-08
Custom 128-bit base UUID format: `0000XXXX-4a47-0000-4763-7365-00000003`

| Characteristic | Short ID | Properties | Data Format | Size |
|----------------|----------|------------|-------------|------|
| System Diagnostics | 0x0301 | Read | DiagnosticsData | 16 bytes |

## Memory Budget (Verified)

### Current State (Before BLE)
- **Internal DRAM used by display:** ~60KB (two DMA transport buffers of 15360 pixels * 2 bytes each)
- **LVGL draw buffer:** ~300KB in PSRAM (already there -- no migration needed)
- **Audio buffers:** ~50KB internal DRAM (I2S + MP3 decode + PCM)
- **Current RAM usage:** 5.3% (from project context)

### After BLE Addition
| Component | Internal DRAM | PSRAM | Notes |
|-----------|--------------|-------|-------|
| NimBLE stack (runtime) | ~50KB | 0 | With optimized MSYS settings, peripheral-only |
| NimBLE task stack | 4KB | 0 | CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE |
| BLE controller | ~20KB | 0 | ESP-IDF BLE controller |
| GATT service table | ~2KB | 0 | Static definitions |
| BLE event queue | ~256 bytes | 0 | 8 events * 32 bytes |
| BLE service logic | ~1KB | 0 | Variables, connection state |
| **Total BLE addition** | **~77KB** | **0** | |
| **Available internal DRAM** | **~170KB+** | **>8MB** | ESP32-S3 has 512KB SRAM total |

The ESP32-S3 has approximately 512KB total SRAM. With current 5.3% usage (~27KB) plus display DMA (~60KB) plus audio (~50KB), there is approximately 375KB free before BLE. Adding ~77KB for BLE leaves ~298KB free -- well within safe margins.

**Risk status:** The roadmap risk about "migrating LVGL display buffers to PSRAM" is **already resolved** -- the main display buffer is allocated with `MALLOC_CAP_SPIRAM`. No additional work needed.

## NVS Partition Architecture (Critical Detail)

The project has TWO NVS partitions:

| Partition | Address | Size | Purpose | Init Function |
|-----------|---------|------|---------|--------------|
| `nvs` | 0x9000 | 16KB | System NVS + NimBLE bond storage | `nvs_flash_init()` -- MUST ADD |
| `nvs_data` | 0x710000 | 64KB | Application data (volume, brightness, jornada) | `nvs_flash_init_partition("nvs_data")` -- EXISTS |

NimBLE bond storage uses `nvs_open("nimble_bond", ...)` which accesses the default `nvs` partition. The application uses `nvs_open_from_partition("nvs_data", ...)`. These are completely separate and will not conflict.

**Action required:** Add `nvs_flash_init()` call in `system_task` BEFORE NimBLE initialization. This is a one-liner but critical for bond persistence.

## Core Affinity and Task Priority Map

| Task | Core | Priority | Stack | Status |
|------|------|----------|-------|--------|
| `system_task` (LVGL + main loop) | 0 | 5 | 8KB | EXISTS |
| `audio_task` (MP3 + I2S) | 1 | 5 | 32KB | EXISTS |
| `ignicao_task` (GPIO polling) | 0 | 2 | 4KB | EXISTS |
| NimBLE host task | 0 | 4 (default) | 4KB | NEW |

NimBLE at priority 4 < system_task at priority 5 ensures LVGL never stutters due to BLE. Audio on Core 1 is completely isolated from BLE on Core 0.

## Open Questions

1. **ble_svc_dis availability in espressif32@6.9.0**
   - What we know: `ble_svc_dis` was added to ESP-IDF in PR #6040 (merged 2020). ESP-IDF 5.3.1 should include it.
   - What's unclear: Whether PlatformIO's `espressif32@6.9.0` package includes the DIS service source file in CMakeLists.txt.
   - Recommendation: Verify during plan 02-01 by checking if `#include "services/dis/ble_svc_dis.h"` compiles. If not, implement DIS as a custom service (simple, ~30 lines).
   - Confidence: MEDIUM -- likely included but needs build verification.

2. **Protocol Version Characteristic (BLE-08)**
   - What we know: The requirement asks for a "protocol version" in Device Info. SIG 0x180A has Software Revision (0x2A28) which can hold this.
   - What's unclear: Whether to use the SIG Software Revision string or add a custom characteristic with a structured version number.
   - Recommendation: Use Software Revision (0x2A28) with format "BLE-P1.0" for simplicity. A custom binary version characteristic is overkill for v1.

3. **GATT-09: Write Validation Scope**
   - What we know: Phase 2 has no writable characteristics (journey/ignition/diagnostics are read+notify only). Write characteristics come in Phase 3 (Configuration Service).
   - What's unclear: Whether GATT-09 validation should be scaffolded now or fully implemented in Phase 3.
   - Recommendation: Implement the validation pattern (bounds checking in access callbacks) now as a reusable helper function, but apply it to actual writable characteristics in Phase 3. Phase 2 planner should create the validation utility.

## Sources

### Primary (HIGH confidence)
- [ESP-IDF bleprph example](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/bleprph) -- Complete GATT server with security, advertising, bonding
- [ESP-IDF bleprph walkthrough](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/bleprph/tutorial/bleprph_walkthrough.md) -- Detailed tutorial with code snippets
- [ESP-IDF GATT Data Exchange documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/ble/get-started/ble-data-exchange.html) -- Official GATT server patterns
- [ESP-IDF NimBLE Kconfig.in](https://github.com/espressif/esp-idf/blob/master/components/bt/host/nimble/Kconfig.in) -- All sdkconfig option names, defaults, descriptions
- [ESP-IDF NimBLE bleprph gatt_svr.c](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/bleprph/main/gatt_svr.c) -- Reference GATT service definition code
- Existing codebase analysis: `esp_bsp.c`, `lv_port.c`, `nvs_manager.cpp`, `main.cpp`, all interfaces (HIGH confidence)

### Secondary (MEDIUM confidence)
- [NimBLE NVS bond store source](https://github.com/espressif/esp-nimble/blob/nimble-1.3.0-idf/nimble/host/store/config/src/ble_store_nvs.c) -- Confirmed default `nvs` partition, `nimble_bond` namespace
- [NimBLE DIS PR #6040](https://github.com/espressif/esp-idf/pull/6040) -- Confirmed `ble_svc_dis` availability
- [ESP-IDF NimBLE memory discussion](https://esp32.com/viewtopic.php?t=33783) -- Runtime memory footprint estimates
- [NimBLE MTU negotiation issues](https://github.com/espressif/esp-idf/issues/9627) -- MTU behavior and limitations
- [Apache Mynewt NimBLE GATT server reference](https://mynewt.apache.org/latest/network/ble_hs/ble_gatts.html) -- `ble_gatts_notify_custom()` API

### Tertiary (LOW confidence)
- [DMC Blog: NimBLE GATT Server](https://www.dmcinfo.com/blog/19377/creating-a-gatt-server-with-esp-idfs-latest-bluetooth-le-stack-nimble/) -- Third-party tutorial, patterns confirmed against official examples
- NimBLE memory footprint (47KB IRAM + 14KB DRAM + 88KB heap) -- Forum report, not officially documented

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- NimBLE is ESP-IDF built-in, all APIs verified against official examples and Kconfig source
- Architecture: HIGH -- Patterns derived from official `bleprph` example and codebase analysis
- sdkconfig options: HIGH -- Verified against actual Kconfig.in source file with defaults and descriptions
- NVS partition behavior: HIGH -- Verified against `ble_store_nvs.c` source (uses default `nvs` partition)
- Memory budget: MEDIUM -- Heap numbers from community reports; verified LVGL PSRAM allocation from codebase
- Pitfalls: HIGH -- Derived from official issues, community reports, and codebase analysis
- ble_svc_dis availability: MEDIUM -- PR merged in 2020, should be in ESP-IDF 5.3.1, needs build verification

**Research date:** 2026-02-10
**Valid until:** 2026-03-10 (stable -- NimBLE API and ESP-IDF 5.3.1 are not changing)
