# Phase 4: OTA (via Wi-Fi) - Research

**Researched:** 2026-02-10
**Domain:** ESP32-S3 OTA firmware update via Wi-Fi HTTP, BLE provisioning, rollback safety
**Confidence:** HIGH (ESP-IDF OTA/Wi-Fi/HTTP APIs are mature and well-documented; verified against official docs and community patterns)

<user_constraints>
## User Constraints (from phase context)

### Locked Decisions
1. **OTA via Wi-Fi** (NOT BLE as originally written in roadmap) -- the decision has changed
2. **BLE Provisioning** -- App sends Wi-Fi SSID/password via BLE characteristic, ESP32 connects as STA
3. **App uploads firmware to ESP32** -- App mobile does HTTP POST to ESP32's local IP (not ESP32 downloading from server)
4. **Disable BLE during OTA** -- Free ~50KB RAM, avoid RF coexistence issues
5. **OTA flow**: App sends Wi-Fi creds via BLE -> ESP32 connects Wi-Fi -> reports IP via BLE -> disables BLE -> App sends firmware via HTTP -> ESP32 writes to OTA partition -> verifies SHA-256 -> reboots -> self-test -> rollback if failed

### Claude's Discretion
- HTTP server configuration (port, buffer sizes, timeouts)
- OTA buffer size for esp_ota_write chunks
- Self-test diagnostic checklist specifics
- OTA state machine event names and structure
- BLE provisioning characteristic format (SSID+password encoding)
- Wi-Fi connection timeout values
- OTA progress reporting mechanism (since BLE is off during transfer)
- HTTP response format for progress/status

### Deferred Ideas (OUT OF SCOPE)
- OTA via BLE data transfer (replaced by Wi-Fi)
- OTA resume from offset (Wi-Fi is fast enough, not worth complexity)
- Firmware signing / secure boot (can be added later)
- Anti-rollback version checking (eFuse-based, not needed for v2.0)
- Fleet management server (manual per-device update for now)
</user_constraints>

## Summary

Phase 4 implements over-the-air firmware updates using a hybrid BLE+Wi-Fi approach. The mobile app first sends Wi-Fi credentials via an existing BLE connection, then the ESP32 connects to Wi-Fi as a station, reports its IP address back via BLE, shuts down BLE entirely to free ~50KB internal SRAM, starts an HTTP server, and receives the firmware binary via HTTP POST from the mobile app on the local network. This approach is dramatically faster than BLE-based OTA (seconds vs minutes for a 3MB image) and avoids the complexity of BLE chunked transfer protocols.

The ESP-IDF provides all needed components out of the box: `esp_ota_ops.h` for partition management and image writing, `esp_http_server.h` for receiving the firmware via HTTP, `esp_wifi.h` for station mode, and the bootloader's rollback mechanism (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`, already configured). The partition table already has dual OTA partitions (ota_0=3MB, ota_1=3MB, otadata=8KB).

The key challenge is the radio transition sequence: BLE must be completely shut down before Wi-Fi starts, and Wi-Fi must be shut down after OTA completes before rebooting. This is a one-way sequence per OTA session -- BLE is NOT restarted after OTA (the device reboots).

**Primary recommendation:** Use `esp_http_server` (not raw sockets) to receive firmware via a single HTTP POST endpoint (`/update`). Buffer is 4KB -- `httpd_req_recv` reads into buffer, `esp_ota_write` writes to flash, loop until `content_len` is consumed. No need to buffer the entire 3MB image in RAM.

## Standard Stack

### Core
| Component | Source | Purpose | Why Standard |
|-----------|--------|---------|--------------|
| `esp_ota_ops.h` | ESP-IDF 5.3.1 built-in | OTA partition management, image write, boot partition swap | Official ESP-IDF OTA API, mature since v3.x |
| `esp_http_server.h` | ESP-IDF 5.3.1 built-in | HTTP server for firmware upload endpoint | Built-in, lightweight (~4KB stack), already compiled in project |
| `esp_wifi.h` | ESP-IDF 5.3.1 built-in | Wi-Fi STA mode for local network connectivity | Standard ESP-IDF Wi-Fi driver, already compiled (`CONFIG_ESP_WIFI_ENABLED=y`) |
| NimBLE (existing) | Already in project | BLE provisioning characteristics (SSID/password/IP) | Already initialized in Phase 2, add 3 characteristics to existing GATT table |
| LVGL 8.4.0 (existing) | Already in project | OtaScreen with progress bar (`lv_bar`) | `LV_USE_BAR` already enabled, follows existing IScreen pattern |

### Supporting
| Component | Source | Purpose | When to Use |
|-----------|--------|---------|-------------|
| `esp_event.h` | ESP-IDF built-in | Wi-Fi event handling (connected, got IP, disconnected) | Required for Wi-Fi STA init |
| `esp_netif.h` | ESP-IDF built-in | TCP/IP network interface for Wi-Fi STA | Required for Wi-Fi STA init |
| `mbedtls/sha256.h` | ESP-IDF built-in | SHA-256 computation for firmware integrity check | App sends expected hash, ESP32 verifies after write |
| `esp_task_wdt.h` | ESP-IDF built-in | Task watchdog for self-test timeout | 60-second watchdog on first boot after OTA |

### What's NOT Needed
| Instead of | Reason |
|------------|--------|
| `esp_https_ota.h` | Designed for ESP32 downloading FROM a server. We receive FROM the app via HTTP POST -- different direction |
| Raw TCP sockets | `esp_http_server` handles connection management, content-length parsing, error responses |
| `esp_wifi_provisioning` | Over-engineered for our use case. We just need 1 BLE characteristic write with SSID+password |
| Wi-Fi AP (SoftAP) mode | User decided STA mode -- ESP32 joins existing network, app and device on same LAN |

### Installation
No new libraries needed. All components are already compiled into the project:
- `CONFIG_ESP_WIFI_ENABLED=y` (in sdkconfig.esp32s3_idf)
- `CONFIG_HTTPD_MAX_REQ_HDR_LEN=512` (compiled in sdkconfig.h)
- `CONFIG_BT_ENABLED=y` + NimBLE (existing)
- `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` (existing)

**sdkconfig additions needed:** None. All required components are already enabled.

## Architecture Patterns

### Recommended File Structure
```
include/
  config/
    app_config.h          (MODIFY - add OTA/Wi-Fi constants)
    ble_uuids.h           (MODIFY - add OTA provisioning service UUID + characteristics)
  interfaces/
    i_ota.h               (NEW - IOtaService abstract interface)
  services/
    ota/
      ota_service.h       (NEW - OTA state machine + Wi-Fi + HTTP server)
      ota_types.h         (NEW - OtaState enum, OtaEvent struct)
    ble/
      gatt/
        gatt_ota_prov.h   (NEW - OTA provisioning GATT callbacks)
  ui/
    screens/
      ota_screen.h        (NEW - OtaScreen IScreen implementation)
src/
  services/
    ota/
      ota_service.cpp     (NEW - main OTA orchestrator)
      ota_wifi.cpp         (NEW - Wi-Fi STA init/connect/deinit)
      ota_http_server.cpp (NEW - HTTP server setup + firmware receive handler)
      ota_self_test.cpp   (NEW - post-reboot self-test + rollback logic)
    ble/
      gatt/
        gatt_ota_prov.cpp (NEW - GATT provisioning characteristic handlers)
  ui/
    screens/
      ota_screen.cpp      (NEW - progress bar + status text UI)
```

### Pattern 1: OTA State Machine (Central Orchestrator)
**What:** Single `OtaService` class manages the entire OTA lifecycle as an explicit state machine.
**When to use:** Always -- OTA has too many failure modes for ad-hoc control flow.

```cpp
// Source: project convention + ESP-IDF OTA patterns
enum class OtaState : uint8_t {
    IDLE,                  // Normal operation, no OTA in progress
    PROVISIONING,          // BLE characteristic received SSID+password, waiting for user confirm
    CONNECTING_WIFI,       // Wi-Fi STA connecting to AP
    WIFI_CONNECTED,        // Got IP, BLE still active (reporting IP to app)
    DISABLING_BLE,         // Shutting down NimBLE stack
    STARTING_HTTP,         // HTTP server starting on port 8080
    WAITING_FIRMWARE,      // HTTP server running, waiting for POST /update
    RECEIVING,             // Receiving firmware via HTTP POST, writing to flash
    VERIFYING,             // Transfer complete, verifying SHA-256
    REBOOTING,             // Verified OK, rebooting into new firmware
    ABORTING,              // Error occurred, cleaning up (stop HTTP, stop Wi-Fi)
    FAILED                 // Terminal error state, user must retry or reboot
};
```

### Pattern 2: BLE Provisioning via Existing GATT Server
**What:** Add an OTA Provisioning Service (group 4) to the existing GATT table with 3 characteristics.
**When to use:** The provisioning phase -- before Wi-Fi starts and BLE shuts down.

```cpp
// New service in ble_uuids.h (group 4)
// OTA Provisioning Service UUID: 00000400-4A47-0000-4763-7365-00000004

// Characteristics:
// 1. OTA Wi-Fi Credentials (Write): SSID + password
//    Format: [1 byte SSID len][SSID bytes][1 byte PWD len][PWD bytes]
//    Max: 1 + 32 + 1 + 64 = 98 bytes (fits in single BLE write with MTU 512)
//
// 2. OTA Status (Read + Notify): current OtaState + error code
//    Format: [1 byte state][1 byte error_code]
//
// 3. OTA IP Address (Read + Notify): IPv4 address when connected
//    Format: [4 bytes IPv4] or "192.168.1.100" as string
```

**Flow:**
1. App writes Wi-Fi credentials to characteristic 1
2. ESP32 posts event to system_task via existing config event queue pattern
3. system_task starts Wi-Fi connection
4. On IP acquired, ESP32 updates characteristic 3 (IP address) and notifies app
5. App reads IP address
6. ESP32 shuts down BLE (app already has the IP)
7. App connects to ESP32's IP via HTTP

### Pattern 3: HTTP Firmware Receive Handler (Streaming Write)
**What:** Single HTTP POST handler that reads firmware in chunks and writes directly to OTA partition.
**When to use:** The firmware transfer phase -- after BLE is down and HTTP server is running.

```cpp
// Source: ESP-IDF esp_http_server + esp_ota_ops pattern (verified from official docs + Jeija/esp32-softap-ota)
esp_err_t ota_upload_handler(httpd_req_t *req) {
    char buf[4096];  // 4KB buffer -- balance between RAM usage and flash write efficiency
    esp_ota_handle_t ota_handle;
    int remaining = req->content_len;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    int total = remaining;
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;  // Retry on timeout
        } else if (recv_len <= 0) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
            return ESP_FAIL;
        }

        if (esp_ota_write(ota_handle, buf, recv_len) != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash write error");
            return ESP_FAIL;
        }

        remaining -= recv_len;

        // Update progress (post to event queue for UI update)
        int percent = ((total - remaining) * 100) / total;
        ota_post_progress(percent, total - remaining, total);
    }

    if (esp_ota_end(ota_handle) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Image validation failed");
        return ESP_FAIL;
    }

    if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Boot partition set failed");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Rebooting...\"}");

    // Delay to allow HTTP response to be sent, then reboot
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK;
}
```

### Pattern 4: NimBLE Clean Shutdown Sequence
**What:** Proper sequence to completely shut down NimBLE and free ~50KB internal SRAM.
**When to use:** After app has received the ESP32's IP address, before starting HTTP server.

```cpp
// Source: ESP-IDF GitHub issue #3475 (confirmed working), NimBLE official docs
// CRITICAL: Must NOT be called from NimBLE host task context

void ble_shutdown() {
    // 1. Stop advertising first
    ble_gap_adv_stop();

    // 2. Disconnect any active connection
    // (give app time to read IP address first)
    if (conn_handle != 0) {
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        vTaskDelay(pdMS_TO_TICKS(200));  // Wait for disconnect event
    }

    // 3. Stop NimBLE host (blocks until host task exits)
    // Must be called from a task OTHER than the NimBLE host task
    int ret = nimble_port_stop();
    if (ret == 0) {
        // 4. Deinitialize NimBLE port
        nimble_port_deinit();

        // 5. Deinitialize HCI and BT controller
        // ESP-IDF 5.3: esp_nimble_hci_and_controller_deinit() handles both
        ret = esp_nimble_hci_and_controller_deinit();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "HCI/controller deinit failed: %s", esp_err_to_name(ret));
        }
    }

    // After this, BLE memory is freed but CANNOT be reinitialized
    // (device will reboot after OTA anyway)
    ESP_LOGI(TAG, "BLE shutdown complete, heap free: %lu",
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}
```

**Key constraint from GitHub issue #3475:** Do NOT call `esp_nimble_deinit()` -- it is not needed and can cause a hang. Only call `nimble_port_stop()` + `nimble_port_deinit()` + `esp_nimble_hci_and_controller_deinit()`.

### Pattern 5: Wi-Fi STA Init and Connect
**What:** Initialize Wi-Fi in station mode with SSID/password from BLE provisioning.
**When to use:** After BLE provisioning characteristic receives credentials.

```cpp
// Source: ESP-IDF official station example (verified)
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     5

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

bool wifi_sta_init(const char* ssid, const char* password) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection or failure (timeout: 15 seconds)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        return true;  // Connected, IP acquired
    }
    return false;  // Connection failed
}
```

### Pattern 6: Post-OTA Self-Test and Rollback
**What:** On every boot, check if firmware is in PENDING_VERIFY state and run diagnostics.
**When to use:** Called EARLY in `app_main()` / `system_task`, before normal operation.

```cpp
// Source: ESP-IDF OTA official documentation (verified)
#include "esp_ota_ops.h"

void ota_self_test() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "First boot after OTA -- running self-test...");

            bool ok = true;

            // Test 1: LVGL initialized (display renders)
            ok = ok && (lv_disp_get_default() != NULL);

            // Test 2: Touch responsive (I2C communication)
            ok = ok && touch_test_ping();

            // Test 3: NVS accessible
            ok = ok && NvsManager::getInstance()->init();

            // Test 4: BLE stack initializes
            ok = ok && BleService::getInstance()->init();

            // Test 5: Audio system initializes
            ok = ok && (initSimpleAudio() == true);

            if (ok) {
                ESP_LOGI(TAG, "Self-test PASSED -- confirming firmware");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "Self-test FAILED -- rolling back!");
                esp_ota_mark_app_invalid_rollback_and_reboot();
                // Does not return -- reboots into previous firmware
            }
        }
    }
}
```

### Pattern 7: OTA Event Queue for Progress Updates
**What:** Since BLE is off during firmware transfer, use a FreeRTOS event queue to update OtaScreen from HTTP handler.
**When to use:** During firmware receive phase.

```cpp
// Follows existing ble_event_queue.h / gatt_config.h pattern

struct OtaProgressEvent {
    uint8_t percent;       // 0-100
    uint32_t bytes_received;
    uint32_t bytes_total;
    OtaState state;
};

static QueueHandle_t s_ota_progress_queue = NULL;

// Called from HTTP handler (httpd task context)
void ota_post_progress(uint8_t percent, uint32_t received, uint32_t total) {
    if (s_ota_progress_queue == NULL) return;
    OtaProgressEvent evt = { percent, received, total, OtaState::RECEIVING };
    xQueueOverwrite(s_ota_progress_queue, &evt);  // Overwrite -- always show latest
}

// Called from system_task main loop (Core 0, LVGL-safe)
void ota_process_progress(void (*handler)(const OtaProgressEvent& evt)) {
    if (s_ota_progress_queue == NULL || handler == NULL) return;
    OtaProgressEvent evt;
    if (xQueueReceive(s_ota_progress_queue, &evt, 0) == pdTRUE) {
        handler(evt);  // Update OtaScreen UI
    }
}
```

**Key insight:** Use `xQueueOverwrite` (queue size 1) instead of `xQueueSend` -- the UI only cares about the latest progress, not every intermediate value. This avoids queue overflow during fast transfers.

### Anti-Patterns to Avoid

- **Calling `esp_ota_write()` from the httpd task with tiny buffers:** The HTTP server runs its own task. Writing to flash in small chunks (e.g., 256 bytes) is inefficient -- flash writes have overhead per call. Use 4KB buffer minimum.

- **Starting Wi-Fi while BLE is still active:** Even though ESP32-S3 supports coexistence (`CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y`), the combined RAM usage is too high for our constrained environment. Always shut down BLE first.

- **Calling NimBLE shutdown from the NimBLE host task:** `nimble_port_stop()` must be called from a different task (e.g., system_task). It signals the host task to exit and waits for it.

- **Marking firmware valid immediately after reboot:** Always run diagnostics first. If the new firmware has a critical bug, the 60-second watchdog ensures automatic rollback.

- **Blocking system_task during Wi-Fi connection:** Wi-Fi connection can take 5-15 seconds. Use `xEventGroupWaitBits` with a timeout, but process LVGL events in between (or move Wi-Fi wait to a separate task).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| HTTP server | Custom TCP socket server | `esp_http_server` | Connection management, HTTP parsing, content-length tracking, timeout handling all built-in |
| OTA partition management | Manual partition table reads/writes | `esp_ota_ops` API | Handles partition selection, erase, write alignment, image validation, boot config update |
| Wi-Fi event handling | Polling Wi-Fi status in a loop | `esp_event` + `xEventGroupWaitBits` | ESP-IDF event system is the standard pattern, handles all edge cases |
| SHA-256 computation | Custom hash implementation | `mbedtls_sha256` (ESP-IDF built-in) | Hardware-accelerated on ESP32-S3, proven implementation |
| Firmware image validation | Manual header parsing | `esp_ota_end()` | Validates image magic bytes and structure automatically |

**Key insight:** ESP-IDF's OTA+HTTP+Wi-Fi components are battle-tested and handle edge cases (partial writes, timeouts, flash alignment) that a custom implementation would miss. The HTTP server can receive a 3MB firmware with just 4KB of RAM buffer.

## Common Pitfalls

### Pitfall 1: Wi-Fi Connection Blocks LVGL Rendering
**What goes wrong:** `xEventGroupWaitBits()` blocks the calling task while waiting for Wi-Fi connection (5-15 seconds). If called from system_task, LVGL stops rendering and the display freezes.
**Why it happens:** The system_task main loop must call `lv_timer_handler()` every 5ms. Blocking for 15 seconds causes visible UI freeze.
**How to avoid:** Use a non-blocking approach: create a dedicated short-lived task for Wi-Fi connection, or poll the event group with 0 timeout inside the main loop while continuing to process LVGL. The OtaScreen can show a "Connecting..." spinner.
**Warning signs:** Display freezes during "Connecting to Wi-Fi" phase.

### Pitfall 2: NimBLE Shutdown Crash if Called from Wrong Context
**What goes wrong:** `nimble_port_stop()` hangs or crashes if called from within the NimBLE host task.
**Why it happens:** `nimble_port_stop()` signals the host task to exit and waits for task deletion. Calling it from the host task itself creates a deadlock.
**How to avoid:** Always call the BLE shutdown sequence from system_task (Core 0). NimBLE host task runs on Core 0 as well but is a separate FreeRTOS task. The system_task context is safe.
**Warning signs:** Device hangs after "Shutting down BLE" log message.

### Pitfall 3: HTTP Server Stack Overflow During OTA
**What goes wrong:** The HTTP server task runs with default 4096-byte stack. The OTA handler allocates a 4KB buffer on stack, plus `esp_ota_write` needs internal stack space. Total exceeds 4096 bytes.
**Why it happens:** `HTTPD_DEFAULT_CONFIG()` sets `stack_size = 4096`. The OTA handler's local buffer + function call overhead exceeds this.
**How to avoid:** Increase HTTP server stack to 8192 bytes in `httpd_config_t`. Or allocate the OTA buffer from heap instead of stack.
**Warning signs:** Guru Meditation Error (stack overflow) during firmware upload.

### Pitfall 4: esp_ota_begin Fails with ESP_ERR_OTA_ROLLBACK_INVALID_STATE
**What goes wrong:** If the running firmware is in `ESP_OTA_IMG_PENDING_VERIFY` state (first boot after a previous OTA, not yet confirmed), calling `esp_ota_begin()` for a new OTA fails.
**Why it happens:** The rollback mechanism requires the current firmware to be confirmed valid before allowing another OTA.
**How to avoid:** Run `ota_self_test()` early in boot, which calls `esp_ota_mark_app_valid_cancel_rollback()` on success. This transitions the state to `ESP_OTA_IMG_VALID`, allowing future OTA operations.
**Warning signs:** HTTP OTA upload returns 500 with "OTA begin failed" on a device that was recently OTA-updated.

### Pitfall 5: Wi-Fi + LVGL Timer Conflict on Core 0
**What goes wrong:** Wi-Fi task is pinned to Core 0 (`CONFIG_ESP_WIFI_TASK_PINNED_TO_CORE_0=y` in sdkconfig). LVGL timer handler also runs on Core 0 in system_task. Under heavy Wi-Fi traffic (firmware upload), Wi-Fi task can starve LVGL updates.
**Why it happens:** Wi-Fi task has priority 18 (`CONFIG_LWIP_TCPIP_TASK_PRIO=18`), system_task has priority 5. Wi-Fi preempts system_task during transfer.
**How to avoid:** This is acceptable during OTA -- the OtaScreen only needs to update a progress bar, which can tolerate 100-200ms update latency. Do NOT lower Wi-Fi priority. The transfer speed is more important than UI smoothness during OTA.
**Warning signs:** OtaScreen progress bar updates in jerky increments instead of smooth progression.

### Pitfall 6: Content-Length Missing in HTTP POST
**What goes wrong:** If the mobile app sends a chunked transfer encoding (no Content-Length header), `req->content_len` is 0 and the receive loop never executes.
**Why it happens:** Some HTTP clients default to chunked encoding for large bodies.
**How to avoid:** The mobile app MUST send `Content-Length` header with the exact firmware size. Document this as an API requirement. In the handler, validate that `content_len > 0` and `content_len <= OTA_MAX_IMAGE_SIZE` before starting.
**Warning signs:** HTTP handler receives 0 bytes, OTA image is empty.

### Pitfall 7: Device Left in Broken State if OTA Aborted Mid-Transfer
**What goes wrong:** Wi-Fi disconnects or app crashes during firmware upload. The OTA partition has a partial image. BLE is already shut down. The device cannot communicate with the app.
**Why it happens:** BLE was disabled before starting the HTTP transfer. If the transfer fails, there is no way to recover except rebooting.
**How to avoid:** If the HTTP transfer fails: (1) call `esp_ota_abort(ota_handle)`, (2) stop HTTP server, (3) stop and deinit Wi-Fi, (4) reboot. On reboot, BLE reinitializes normally and the app can retry. The partial OTA partition write is harmless -- the boot partition was never changed.
**Warning signs:** Device appears unresponsive after failed OTA -- needs manual reboot.

### Pitfall 8: Forgetting to Lock UI During OTA
**What goes wrong:** User accidentally navigates away from OtaScreen during transfer (menu button, back swipe). The OTA continues in the background but the user sees no progress. They might power cycle the device thinking it is frozen.
**Why it happens:** StatusBar menu/back buttons remain active during OTA.
**How to avoid:** When OTA state leaves IDLE, disable all navigation in ScreenManager. OtaScreen should have no interactive elements except a "Cancel" button (which triggers safe abort). Re-enable navigation only after OTA completes or is aborted.

## Code Examples

### Complete OTA Flow Orchestration (system_task Integration)

```cpp
// In main.cpp system_task main loop, add after existing event processing:

// Process OTA events
if (otaService && otaService->getState() != OtaState::IDLE) {
    otaService->process();  // Drive state machine forward

    // Update OtaScreen with latest progress
    ota_process_progress([](const OtaProgressEvent& evt) {
        if (screenMgr->getCurrentScreen() == ScreenType::OTA) {
            otaScreen.updateProgress(evt.percent, evt.bytes_received, evt.bytes_total);
            otaScreen.updateState(evt.state);
        }
    });
}
```

### HTTP Server Configuration for OTA

```cpp
// Source: ESP-IDF esp_http_server docs
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
config.stack_size = 8192;           // 8KB for OTA handler (default 4KB too small)
config.server_port = 8080;          // Non-standard port to avoid conflicts
config.max_open_sockets = 1;        // Only 1 connection needed for OTA
config.recv_wait_timeout = 30;      // 30 seconds (firmware upload can be slow on weak Wi-Fi)
config.send_wait_timeout = 10;      // 10 seconds for response
config.core_id = 0;                 // Same core as system_task (Wi-Fi is also on Core 0)
config.lru_purge_enable = false;    // No need with 1 socket

httpd_handle_t server = NULL;
httpd_start(&server, &config);

// Register OTA upload endpoint
httpd_uri_t update_uri = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = ota_upload_handler,
    .user_ctx = NULL
};
httpd_register_uri_handler(server, &update_uri);

// Optional: status endpoint for app to check if device is ready
httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = ota_status_handler,  // Returns JSON with device info + OTA state
    .user_ctx = NULL
};
httpd_register_uri_handler(server, &status_uri);
```

### BLE Provisioning Characteristic Write Handler

```cpp
// Follows existing gatt_config.cpp pattern
// Format: [1B ssid_len][ssid_bytes][1B pwd_len][pwd_bytes]

int ota_prov_wifi_creds_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt* ctxt, void* arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }

    // Min 4 bytes: 1 ssid_len + 1 ssid_char + 1 pwd_len + 1 pwd_char
    // Max 98 bytes: 1 + 32 + 1 + 64
    int err = gatt_validate_write(ctxt, 4, 98);
    if (err != 0) return err;

    uint8_t buf[98];
    memset(buf, 0, sizeof(buf));
    int len = gatt_read_write_data(ctxt, buf, sizeof(buf));
    if (len < 4) return BLE_ATT_ERR_UNLIKELY;

    // Parse SSID
    uint8_t ssid_len = buf[0];
    if (ssid_len == 0 || ssid_len > 32 || (1 + ssid_len + 1) > len) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    char ssid[33] = {};
    memcpy(ssid, &buf[1], ssid_len);

    // Parse password
    uint8_t pwd_offset = 1 + ssid_len;
    uint8_t pwd_len = buf[pwd_offset];
    if (pwd_len > 64 || (pwd_offset + 1 + pwd_len) > len) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    char password[65] = {};
    memcpy(password, &buf[pwd_offset + 1], pwd_len);

    // Post to system_task via event queue
    ota_post_wifi_credentials(ssid, password);

    ESP_LOGI(TAG, "OTA Wi-Fi creds received: SSID='%s' (pwd_len=%d)", ssid, pwd_len);
    return 0;
}
```

### SHA-256 Verification Pattern

```cpp
// App sends expected SHA-256 via HTTP header or BLE characteristic before upload
// ESP32 computes running SHA-256 during upload and compares after

#include "mbedtls/sha256.h"

// In OTA upload handler, alongside esp_ota_write:
mbedtls_sha256_context sha_ctx;
mbedtls_sha256_init(&sha_ctx);
mbedtls_sha256_starts(&sha_ctx, 0);  // 0 = SHA-256 (not SHA-224)

// In the receive loop:
mbedtls_sha256_update(&sha_ctx, (const unsigned char*)buf, recv_len);

// After transfer complete:
uint8_t computed_sha[32];
mbedtls_sha256_finish(&sha_ctx, computed_sha);
mbedtls_sha256_free(&sha_ctx);

// Compare with expected hash (sent as X-SHA256 HTTP header or BLE characteristic)
if (memcmp(computed_sha, expected_sha, 32) != 0) {
    ESP_LOGE(TAG, "SHA-256 mismatch! Aborting OTA.");
    esp_ota_abort(ota_handle);
    // ... send error response
}
```

### Wi-Fi Shutdown After OTA (Before Reboot)

```cpp
// Source: ESP-IDF wifi docs
void wifi_shutdown() {
    // 1. Unregister event handlers first (prevent reconnect loops)
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);

    // 2. Stop Wi-Fi
    esp_wifi_stop();

    // 3. Deinitialize Wi-Fi
    esp_wifi_deinit();

    // 4. Destroy default network interface
    esp_netif_destroy_default_wifi(s_sta_netif);

    // 5. Destroy event loop (optional before reboot)
    esp_event_loop_delete_default();

    ESP_LOGI(TAG, "Wi-Fi shutdown complete");
}
```

## State of the Art

| Old Approach (Roadmap v1) | Current Approach (Locked) | Reason for Change | Impact |
|---------------------------|---------------------------|-------------------|--------|
| OTA via BLE GATT chunks | OTA via Wi-Fi HTTP POST | BLE is 10-50x slower (3-5 min vs 10-30 sec for 3MB). BLE chunking protocol is complex. Wi-Fi HTTP is simple. | Simpler code, much faster transfers, better user experience |
| BLE coexistence during OTA | Disable BLE during OTA | Frees ~50KB RAM, eliminates RF coexistence issues | More RAM for HTTP server + OTA buffers |
| ESP32 downloads from server | App uploads to ESP32 | Works on local network without internet. App has the firmware binary. | No server infrastructure needed |

**Deprecated patterns from earlier research:**
- BLE OTA Data characteristic (write-without-response): NOT needed -- Wi-Fi HTTP replaces this
- BLE OTA Control characteristic (start/abort/commit): Replaced by HTTP endpoints + BLE provisioning
- BLE MTU optimization for OTA throughput: Irrelevant -- Wi-Fi has orders of magnitude more bandwidth
- OTA chunk buffering in NimBLE context: Irrelevant -- httpd handles buffering
- BLE connection drop resume protocol: Not needed -- Wi-Fi is more reliable and faster to retry

## Open Questions

1. **Wi-Fi connection blocking vs LVGL rendering**
   - What we know: `xEventGroupWaitBits()` blocks the calling task. If called from system_task, LVGL freezes.
   - What's unclear: Best pattern for non-blocking Wi-Fi connection while keeping LVGL alive.
   - Recommendation: Poll the event group with `pdMS_TO_TICKS(0)` timeout inside the main loop, alternating with `lv_timer_handler()`. Or create a temporary Wi-Fi connection task that posts result to system_task via event queue.

2. **Exact NimBLE shutdown RAM recovery**
   - What we know: NimBLE uses ~50KB internal SRAM. Community reports vary from 40-60KB.
   - What's unclear: Exact amount freed after full shutdown sequence on this specific firmware build.
   - Recommendation: Log `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` before and after BLE shutdown during development. Expect ~40-55KB freed.

3. **HTTP server port selection**
   - What we know: Default is port 80. The device is on a local network.
   - What's unclear: Whether port 80 is preferred (standard) or 8080 (avoids potential conflicts with router admin interfaces).
   - Recommendation: Use port 8080 for safety. Document in app protocol.

4. **Progress reporting to app during transfer (no BLE)**
   - What we know: BLE is off during firmware upload. The HTTP response is sent only after the transfer completes.
   - What's unclear: How the app knows upload progress percentage during transfer.
   - Recommendation: The app can track progress based on bytes sent vs total file size (client-side progress). The device does not need to report progress to the app. The OtaScreen shows progress locally. Optionally, add a `/progress` GET endpoint the app can poll.

5. **OTA self-test touch test implementation**
   - What we know: Touch input uses I2C bus. The touch controller should respond to a read command.
   - What's unclear: Whether there is an existing API to "ping" the touch controller.
   - Recommendation: Read touch controller ID register via I2C. If read succeeds, touch hardware is working. This does not test the full touch pipeline but validates I2C bus and controller power.

## Memory Budget Analysis

### Current State (Phase 3 complete, BLE running)
| Component | Internal SRAM (est.) |
|-----------|---------------------|
| FreeRTOS kernel + tasks | ~30KB |
| LVGL (core, no display buffers) | ~20KB |
| NimBLE host + controller | ~50KB |
| NVS cache | ~5KB |
| Audio buffers | ~10KB (Core 1) |
| Application code stacks | ~25KB |
| **Total used** | **~140KB** |
| **Free internal SRAM** | **~80-100KB** |

### During OTA (BLE off, Wi-Fi on)
| Component | Internal SRAM (est.) |
|-----------|---------------------|
| FreeRTOS kernel + tasks | ~30KB |
| LVGL (core) | ~20KB |
| Wi-Fi driver + buffers | ~50KB |
| HTTP server + OTA handler | ~12KB (8KB stack + 4KB buffer) |
| LwIP TCP/IP stack | ~10KB |
| Application code stacks | ~15KB (no audio during OTA) |
| NVS cache | ~5KB |
| **Total used** | **~142KB** |
| **Free internal SRAM** | **~78-98KB** |

**Conclusion:** Memory budget is viable. Freeing BLE (~50KB) before starting Wi-Fi (~50KB) keeps total usage roughly the same. The 4KB OTA buffer is modest. The HTTP server adds ~12KB total overhead.

**Note:** Display buffers are in PSRAM, not counted above. PSRAM has 8MB available.

## Timing Estimates

| Phase | Duration (est.) |
|-------|----------------|
| BLE provisioning (write credentials) | < 1 second |
| Wi-Fi STA connection | 3-10 seconds |
| BLE shutdown | < 1 second |
| HTTP server start | < 100ms |
| Firmware transfer (3MB over Wi-Fi) | 5-15 seconds (depends on Wi-Fi quality) |
| SHA-256 verification | < 1 second (hardware-accelerated) |
| Flash commit + boot partition swap | < 500ms |
| Reboot | 2-3 seconds |
| Self-test | 3-5 seconds |
| **Total OTA time** | **~15-35 seconds** |

Compared to BLE OTA: 3-10 minutes for 3MB at ~10-15 KB/s throughput.

## Constants to Add to app_config.h

```cpp
// ============================================================================
// CONFIGURACOES DE OTA (Wi-Fi)
// ============================================================================

#define OTA_HTTP_SERVER_PORT        8080
#define OTA_HTTP_STACK_SIZE         8192
#define OTA_HTTP_RECV_TIMEOUT_S     30
#define OTA_RECEIVE_BUFFER_SIZE     4096
#define OTA_MAX_IMAGE_SIZE          (3 * 1024 * 1024)   // 3MB (partition size)
#define OTA_SELF_TEST_TIMEOUT_S     60                   // Already exists
#define OTA_WIFI_CONNECT_TIMEOUT_MS 15000
#define OTA_WIFI_MAX_RETRY          5
#define OTA_REBOOT_DELAY_MS         500
#define OTA_BLE_DISCONNECT_DELAY_MS 200

// ============================================================================
// CONFIGURACOES DE TASKS - OTA
// ============================================================================

#define OTA_TASK_CORE               0
#define OTA_TASK_PRIORITY           6       // Higher than system_task during OTA
#define OTA_TASK_STACK_SIZE         (8 * 1024)
```

## Sources

### Primary (HIGH confidence)
- [ESP-IDF OTA API documentation (stable/v5.5.2)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html) -- esp_ota_begin/write/end/set_boot_partition, rollback mechanism, state diagram
- [ESP-IDF HTTP Server API (stable)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html) -- httpd_start, httpd_req_recv, URI handler pattern, config
- [ESP-IDF Wi-Fi Station example (GitHub)](https://github.com/espressif/esp-idf/blob/master/examples/wifi/getting_started/station/main/station_example_main.c) -- complete Wi-Fi STA init code
- [ESP-IDF OTA API docs (ESP32-S3 specific)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html) -- rollback, self-test, state verification
- [esp32-softap-ota GitHub project](https://github.com/Jeija/esp32-softap-ota) -- verified HTTP OTA handler pattern with httpd_req_recv + esp_ota_write loop

### Secondary (MEDIUM confidence)
- [ESP-IDF NimBLE Host APIs (stable/ESP32-S3)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/bluetooth/nimble/index.html) -- esp_nimble_hci_deinit
- [ESP-IDF GitHub Issue #3475: NimBLE clean shutdown](https://github.com/espressif/esp-idf/issues/3475) -- confirmed working: nimble_port_stop + nimble_port_deinit (DO NOT call esp_nimble_deinit)
- [ESP32 Forum: enabling/disabling NimBLE server](https://www.esp32.com/viewtopic.php?t=32972) -- NimBLE shutdown/restart patterns
- [ESP-IDF ESP32-S3 RAM usage guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/performance/ram-usage.html) -- Wi-Fi buffer optimization strategies
- [ESP-IDF Wi-Fi documentation (stable)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html) -- STA mode, event handling
- Project codebase: `sdkconfig.esp32s3_idf` -- verified CONFIG_ESP_WIFI_ENABLED=y, CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y, CONFIG_HTTPD_*, CONFIG_BT_NIMBLE_* already configured

### Tertiary (LOW confidence)
- Wi-Fi stack RAM consumption (~50KB) -- widely cited but exact numbers vary by configuration. Validate with `heap_caps_get_free_size()` at runtime.
- NimBLE RAM recovery after shutdown (~40-55KB) -- based on community reports. Validate empirically.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all components are ESP-IDF built-in, already compiled in project
- Architecture: HIGH -- patterns follow existing codebase conventions (IScreen, event queues, GATT modules)
- OTA API usage: HIGH -- esp_ota_ops is stable since ESP-IDF v3.x, verified against current docs
- HTTP server: HIGH -- esp_http_server is well-documented, verified handler pattern
- Wi-Fi STA: HIGH -- standard ESP-IDF pattern, verified from official example
- NimBLE shutdown: MEDIUM -- correct sequence confirmed by GitHub issue, but timing edge cases may exist
- Memory budget: MEDIUM -- estimates based on community data, needs runtime validation
- Pitfalls: HIGH -- catalogued from earlier research + official documentation + new Wi-Fi-specific issues

**Research date:** 2026-02-10
**Valid until:** 2026-04-10 (ESP-IDF OTA/HTTP/Wi-Fi APIs are very stable)
