# Domain Pitfalls

**Domain:** ESP32-S3 BLE (NimBLE) + LVGL Multi-Screen Navigation + BLE OTA Firmware Update
**Project:** Teclado de Jornada Digital
**Researched:** 2026-02-09
**Overall Confidence:** MEDIUM (based on training data for ESP-IDF 5.x / NimBLE / LVGL 8.x; WebSearch unavailable for live verification)

---

## Critical Pitfalls

Mistakes that cause hard-to-diagnose crashes, data corruption, or require architectural rewrites.

---

### Pitfall 1: NimBLE + LVGL RAM Starvation on Internal SRAM

**What goes wrong:** Enabling BLE (even NimBLE, the lightweight stack) consumes 50-80 KB of internal SRAM. The current project allocates display buffers via `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA` in `esp_bsp.c`, with `DISPLAY_BUFFER_SIZE = 480 * 320 = 153,600 pixels` (~300 KB at 16-bit). Combined with minimp3 decoder buffers (8 KB MP3 buffer + PCM buffer + ~6 KB decoder struct, all allocated with `MALLOC_CAP_INTERNAL` in `simple_audio_manager.cpp`), and FreeRTOS task stacks (system_task 8 KB + audio_task 32 KB + ignicao_task 4 KB = 44 KB), the system approaches the ~320 KB usable internal SRAM limit. NimBLE controller requires DMA-capable internal RAM for ACL/HCI buffers -- PSRAM will not work for these.

**Why it happens:** BLE is currently disabled (`CONFIG_BT_ENABLED is not set` in sdkconfig). Developers enable it without auditing the internal RAM budget. The display buffer is the single largest consumer and is the only one that can realistically move to PSRAM.

**Consequences:**
- `heap_caps_malloc` returns NULL during BLE initialization
- Random crashes during BLE operations when LVGL is rendering
- Watchdog timeouts on Core 0 when both LVGL timer handler and NimBLE host task compete for allocations

**Prevention:**
1. **Move LVGL display buffers to PSRAM.** The ESP32-S3 PSRAM is AHB-GDMA capable (confirmed: `CONFIG_SOC_AHB_GDMA_SUPPORT_PSRAM=y` in sdkconfig). Modify `esp_bsp.c` to use `MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA` for display buffer allocation
2. **Audit internal RAM before enabling BLE.** Add this to `app_main()`:
   ```c
   ESP_LOGI(TAG, "Internal free: %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
   ESP_LOGI(TAG, "DMA free: %d", heap_caps_get_free_size(MALLOC_CAP_DMA));
   ```
3. **Configure NimBLE with minimal connection parameters:** 1 connection max, small ACL buffers (`CONFIG_BT_NIMBLE_ACL_BUF_COUNT=12`, `CONFIG_BT_NIMBLE_ACL_BUF_SIZE=255`)
4. **Reserve at minimum 80 KB of internal SRAM** for NimBLE before other subsystem allocations
5. **Move audio MP3 read buffer to PSRAM** -- only the PCM DMA buffer needs internal RAM. The 8 KB MP3 input buffer (`audio->mp3_buffer` at line 213 of `simple_audio_manager.cpp`) can safely use PSRAM

**Detection:**
- Log `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` at key checkpoints (after display init, after audio init, after BLE init)
- Enable `CONFIG_HEAP_TRACING_STANDALONE` during development
- Watch for `E (xxx) BT_CONTROLLER: allocate memory failed` in serial logs

**Phase:** Must be resolved in Phase 1 (BLE Foundation) BEFORE writing any BLE code. Memory audit and PSRAM migration is prerequisite infrastructure work.

---

### Pitfall 2: LVGL Screen Object Leaks on Navigation

**What goes wrong:** The current codebase creates LVGL screens with `lv_obj_create(NULL)` (line 122 of `button_manager.cpp`) and navigates using `lv_scr_load_anim()` with `auto_del=false` (line 179: `lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false)`). The `false` parameter means the previous screen is NOT auto-deleted. With the current 2-screen setup (numpad + jornada keyboard), this is barely noticeable. When adding 4-6 new screens (BLE settings, OTA progress, diagnostics, driver info), each navigation creates ~20-40 KB of LVGL objects that are never freed.

**Why it happens:** LVGL 8.x does NOT garbage-collect screens. They persist in memory until explicitly deleted with `lv_obj_del()`. The existing `ScreenManager` class (in `jornada_keyboard.h`, lines 116-146) only manages 2 screens and does not implement screen cleanup on exit.

**Consequences:**
- Progressive memory leak -- device works for a while, then crashes after many navigations
- LVGL `malloc` failures cause blank screens or corrupted rendering
- Timers attached to off-screen objects (like `statusUpdateTimer` in ButtonManager) continue firing, wasting CPU and potentially causing use-after-free if the screen is eventually deleted

**Prevention:**
1. **Use `auto_del=true` in `lv_scr_load_anim()`** OR manually delete the previous screen in the `LV_EVENT_SCREEN_LOADED` callback on the new screen
2. **Implement screen lifecycle management:**
   - Each screen class implements `onCreate()` / `onResume()` / `onPause()` / `onDestroy()`
   - `onPause()` stops timers and detaches callbacks
   - `onDestroy()` calls `lv_obj_del(screen)` which recursively deletes all children
   - At most 2 screens exist simultaneously (current + previous during animation transition)
3. **Clean up timers before screen deletion.** The current `ButtonManager` has `statusUpdateTimer`, `statusTimer`, and `retryTimer` -- all must be deleted via `lv_timer_del()` before the screen is destroyed
4. **Register `LV_EVENT_SCREEN_UNLOADED` callback** to trigger cleanup of the outgoing screen
5. **Never store raw `lv_obj_t*` pointers** to child objects of a screen that may be deleted -- invalidate all references in `onDestroy()`

**Detection:**
- Enable `LV_USE_MEM_MONITOR 1` in `lv_conf.h` (currently disabled) to display real-time memory usage
- Log available heap before and after each screen transition
- Stress test: navigate through all screens 50 times and verify memory remains stable

**Phase:** Must be the FIRST implementation in Phase 2 (Multi-Screen Navigation). Do NOT add new screens without this lifecycle infrastructure.

---

### Pitfall 3: Partition Table Migration -- Irreversible One-Way Door

**What goes wrong:** The current partition table has a single `factory` app partition (3 MB, starting at 0x10000) with no OTA support and no `otadata` partition. Switching to an OTA-capable layout requires reflashing the ENTIRE device via USB (partition table + bootloader + app + filesystem). The partition table itself is NOT updatable via OTA. This means:
1. Every existing field device needs a one-time USB reflash
2. If the new partition table is miscalculated, devices can brick
3. The transition is irreversible -- you cannot go back to the old layout via OTA

**Why it happens:** Developers plan to "add OTA later" without understanding that the partition table is the foundation. OTA updates can only replace app partitions within the existing table. Changing the table requires physical access.

**Consequences:**
- All deployed devices need USB access for the migration flash
- Miscalculated partition sizes cause `esp_ota_begin()` failures with `ESP_ERR_OTA_PARTITION_CONFLICT`
- If secure boot is later desired, the bootloader must also be updated during this one-time reflash (cannot be added via OTA)

**Prevention:**
1. **Calculate firmware size with BLE + OTA first.** Build the complete firmware and measure the `.bin` size. Current app without BLE is approximately 2.5 MB; with NimBLE expect +200-400 KB. Design OTA partitions with at least 500 KB headroom
2. **Proposed OTA partition table for 16 MB flash:**
   ```
   # Name,    Type, SubType,  Offset,    Size,      Flags
   nvs,       data, nvs,      0x9000,    0x5000,
   otadata,   data, ota,      0xe000,    0x2000,
   phy_init,  data, phy,      0x10000,   0x1000,
   ota_0,     app,  ota_0,    0x20000,   0x380000,  # 3.5 MB
   ota_1,     app,  ota_1,    0x3A0000,  0x380000,  # 3.5 MB
   littlefs,  data, spiffs,   0x720000,  0x100000,  # 1 MB
   ```
   Total used: ~8.25 MB of 16 MB. Leaves ~7.75 MB unused for future expansion (larger apps, additional data partitions)
3. **Include the OTA partition table in the FIRST BLE-enabled firmware release** -- this firmware must be flashed via USB
4. **Document the one-time USB reflash procedure** for field devices with step-by-step instructions
5. **Consider enabling secure boot v2 and flash encryption** during this same reflash -- these cannot be added later without physical access

**Detection:**
- Verify partition table on device: `esptool.py --port /dev/ttyACM0 read_flash 0x8000 0xC00 partition_dump.bin`
- Compare firmware `.bin` size against OTA partition size before deployment
- Test the full OTA cycle (ota_0 -> ota_1 -> ota_0) on development hardware before any field deployment

**Phase:** Must be finalized and applied in Phase 1 (BLE Foundation). This is a one-way door that gates all subsequent OTA capability.

---

### Pitfall 4: BLE OTA Transfer Corruption Without Application-Layer Checksums

**What goes wrong:** BLE transfers firmware data in small MTU-sized chunks (typically 244 bytes payload with MTU 247, or as low as 20 bytes without MTU negotiation). The BLE link layer provides CRC for individual packets, but does NOT guarantee end-to-end integrity across the entire firmware image. Packet loss, connection drops, mobile app bugs, or BLE stack buffer overflows can silently corrupt the firmware image. Without secure boot, `esp_ota_end()` only validates the image header format, not the full binary integrity.

**Why it happens:** Developers trust the BLE link-layer CRC to cover the full transfer. They write received data directly to the OTA partition chunk-by-chunk without tracking a running hash. ESP-IDF's `esp_ota_end()` only validates SHA-256 if `CONFIG_SECURE_BOOT_V2_ENABLED` or `CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT` is enabled -- neither is currently set.

**Consequences:**
- Device boots corrupted firmware, enters crash loop
- ESP-IDF rollback triggers (if enabled), but user experience is terrible -- "update failed, please try again"
- Without rollback configured, device is bricked until USB reflash
- In a fleet, this causes costly field service visits

**Prevention:**
1. **Implement application-layer integrity verification:**
   - Mobile app sends firmware metadata first: total size, SHA-256 hash, version string, build timestamp
   - ESP32 writes chunks to OTA partition and computes running SHA-256
   - After complete transfer, compare computed hash with the metadata hash
   - Only if match: call `esp_ota_end()` then `esp_ota_set_boot_partition()`
2. **Implement chunked acknowledgment with CRC:**
   - Send ACK with running CRC-32 after every N chunks (e.g., every 4 KB)
   - Mobile app verifies running CRC matches its own calculation
   - On mismatch, retransmit the affected block
3. **Enable rollback:** `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`
4. **Require self-validation after OTA reboot:** new firmware must call `esp_ota_mark_app_valid_cancel_rollback()` only after passing a self-test (display init, BLE init, touch response)
5. **Enable signed app verification:** `CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT=y` at minimum to reject tampered binaries

**Detection:**
- Deliberately corrupt 1 byte in an OTA transfer and verify the device rejects it
- Verify rollback by flashing a firmware that intentionally crashes on boot
- Monitor `esp_ota_get_state_partition()` status after each OTA update

**Phase:** Protocol design in Phase 1 (because partition table and bootloader config are set once). Implementation in Phase 3 (BLE OTA).

---

### Pitfall 5: NimBLE Host Task on Wrong Core Causes LVGL Deadlock

**What goes wrong:** NimBLE's host task processes GAP/GATT events and invokes application callbacks. If NimBLE runs on Core 0 (default: `CONFIG_BT_NIMBLE_PINNED_TO_CORE_0=y`), and a BLE callback tries to update UI by acquiring the display lock, it deadlocks with `system_task` which holds the display lock while running `lv_timer_handler()` (line 184 of `main.cpp`, every 5ms). This is a classic priority inversion scenario.

**Why it happens:** NimBLE default configuration pins to Core 0. The current `system_task` runs on Core 0 at priority 5 and calls `lv_timer_handler()` continuously. Even without directly calling LVGL from BLE callbacks, if NimBLE host task preempts `system_task` during a display-locked section, and the BLE callback blocks waiting for the same lock, both tasks stall.

**Consequences:**
- Complete system freeze (both LVGL and BLE stop responding)
- Watchdog reset after 5-8 seconds with cryptic backtrace
- Intermittent -- works fine in testing with low BLE traffic, fails under OTA data load

**Prevention:**
1. **Pin NimBLE host task to Core 1:** Set `CONFIG_BT_NIMBLE_PINNED_TO_CORE_1=y` in sdkconfig. Core 1 already runs the audio task (priority 5), and NimBLE host has relatively low CPU usage except during OTA
2. **NEVER update LVGL objects from BLE callbacks.** Instead use a FreeRTOS event queue:
   ```c
   // In BLE callback (any core):
   BleEvent evt = { .type = BLE_EVT_OTA_PROGRESS, .data = { .progress = 42 } };
   xQueueSend(ble_event_queue, &evt, 0);

   // In system_task on Core 0 (already holds display lock for LVGL):
   BleEvent evt;
   if (xQueueReceive(ble_event_queue, &evt, 0) == pdTRUE) {
       update_ota_progress_ui(evt.data.progress);  // Safe LVGL call
   }
   ```
3. This mirrors the existing audio pattern: `playAudioFile()` sends to a queue, `audio_task` processes on Core 1
4. The NimBLE controller task (BLE LL) should remain on Core 0 for timing accuracy, but host task can be on either core

**Detection:**
- Enable `CONFIG_FREERTOS_USE_TRACE_FACILITY` and `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` to monitor CPU usage per core
- Add `esp_task_wdt_add()` on both system_task and NimBLE host task
- If system freezes, GDB backtrace shows two tasks blocked on the same mutex

**Phase:** Phase 1 (BLE Foundation). Core pinning and event queue architecture must be decided BEFORE writing any BLE application code.

---

## Moderate Pitfalls

Mistakes that cause significant rework or degraded functionality but are recoverable.

---

### Pitfall 6: BLE MTU Not Negotiated -- OTA Takes 40 Minutes Instead of 4

**What goes wrong:** Default BLE MTU is 23 bytes (20 bytes usable payload). Without explicit MTU negotiation and DLE (Data Length Extension), transferring a 3.5 MB firmware image requires ~175,000 individual BLE write operations. At ~50 writes/second with acknowledgment, this takes 40+ minutes. With MTU 247 and DLE, the same transfer takes 3-5 minutes.

**Why it happens:** NimBLE does not automatically negotiate higher MTU. The peripheral or central must explicitly request it after connection. Developers forget this step or configure it in the wrong place.

**Prevention:**
1. Set in sdkconfig: `CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=517`
2. Enable DLE: `CONFIG_BT_NIMBLE_LL_CFG_FEAT_DATA_LEN_EXT=y`
3. After connection, initiate MTU exchange from ESP32: `ble_gattc_exchange_mtu(conn_handle, mtu_cb, NULL)`
4. Use "Write Without Response" (write command) for OTA data characteristic -- avoids ACK round-trip per packet, roughly doubles throughput
5. Request fast connection interval during OTA: 7.5ms (minimum BLE allows)
6. Document mobile app requirements: Android must call `requestMtu(517)`, iOS auto-negotiates up to ~185

**Detection:**
- Log negotiated MTU in `BLE_GAP_EVENT_MTU` handler
- Time the OTA transfer: if >10 minutes for 3.5 MB, MTU negotiation likely failed
- Compare actual throughput vs theoretical: at MTU 247 with DLE and 7.5ms interval, expect ~12-15 KB/s

**Phase:** MTU configuration in Phase 1 (BLE Foundation). Throughput validation in Phase 3 (BLE OTA).

---

### Pitfall 7: Screen Transition Crash When BLE Event Fires Mid-Animation

**What goes wrong:** `lv_scr_load_anim()` performs animated transitions (~300ms in current code, line 179 of `button_manager.cpp`). During animation, both old and new screens exist and LVGL renders both. If a BLE event triggers a screen change during an active animation (e.g., BLE disconnect forces navigation to "disconnected" screen), LVGL enters an inconsistent state. The animation callback references objects from the previous transition that were deleted or modified.

**Why it happens:** BLE events are asynchronous and can arrive at any time. Without a navigation guard, two screen transitions fire simultaneously. LVGL 8.x does not queue transitions -- calling `lv_scr_load_anim()` during an active animation aborts the first, potentially leaving orphaned LVGL objects and dangling timer callbacks.

**Prevention:**
1. **Implement a navigation state machine:**
   ```
   enum NavState { NAV_IDLE, NAV_TRANSITIONING };
   - NAV_IDLE: accept navigation requests immediately
   - NAV_TRANSITIONING: queue the request, execute after LV_EVENT_SCREEN_LOADED
   ```
2. **Never trigger navigation from BLE callbacks** -- always route through the event queue (Pitfall 5)
3. **Register `LV_EVENT_SCREEN_LOADED`** to set NavState back to IDLE and process any queued navigation
4. **Coalesce rapid navigation requests:** store only the most recent target screen, discard intermediate requests

**Detection:**
- Simulate rapid BLE connect/disconnect cycles while navigating between screens
- Enable `LV_USE_LOG 1` and `LV_USE_ASSERT_OBJ 1` during development to catch invalid object access
- Stress test: 100 rapid screen transitions with simulated BLE events

**Phase:** Phase 2 (Multi-Screen Navigation). Navigation state machine must exist before BLE events are connected to UI transitions.

---

### Pitfall 8: BLE Secure Connections Incompatible with Some Mobile Devices

**What goes wrong:** The project requires LE Secure Connections (LESC) with encryption. If configured to require LESC only, devices with older Android versions (< 6.0 / API 23) or non-standard BLE implementations cannot pair. If configured to accept both LESC and legacy pairing for compatibility, security is weakened -- legacy pairing is vulnerable to passive eavesdropping.

**Why it happens:** Developers test with their own modern phone and it works. In the field, truck drivers may use budget Android devices running older OS versions.

**Prevention:**
1. **LESC-only policy:** `CONFIG_BT_NIMBLE_SM_LEGACY=n`, `CONFIG_BT_NIMBLE_SM_SC=y`
2. **Pairing method:** Use "Just Works" LESC for simplicity (no user interaction needed, still uses ECDH for key exchange). The device has a numpad and display, so passkey display/entry is technically possible but adds UX complexity
3. **Document minimum requirements:** Android 6.0+ (API 23), iOS 9+
4. **Test with diverse devices:** recent iPhone, recent Samsung, budget Xiaomi/Motorola, Android Go device
5. **Store bonds in NVS** so pairing happens only once per phone. Handle bond deletion gracefully (if user resets phone)

**Detection:**
- BLE sniffer (nRF Sniffer) to verify LESC is actually negotiated
- Check `BLE_GAP_EVENT_ENC_CHANGE` for `enc_change.status` errors
- Test with nRF Connect app on multiple Android versions

**Phase:** Phase 1 (BLE Foundation). Security configuration is a one-time decision that affects all subsequent BLE features.

---

### Pitfall 9: OTA Connection Drop -- No Resume, Restart from Zero

**What goes wrong:** BLE connection drops at 90% of OTA transfer (3.15 MB of 3.5 MB). The entire transfer restarts from byte 0. For a 4-minute transfer, losing 3.5 minutes of progress is frustrating. Users may abandon the update after repeated failures.

**Why it happens:** Simple OTA implementations write sequentially from offset 0 and have no memory of progress. On reconnect, the mobile app restarts the transfer.

**Prevention:**
1. **Design resumable protocol from V1:**
   - Store current write offset in NVS every 64 KB (coarse granularity to minimize NVS wear)
   - On reconnect, ESP32 sends `OTA_RESUME_OFFSET` to mobile app via BLE characteristic
   - Mobile app resumes from that offset
   - Include running CRC-32 at each checkpoint for validation
2. **Aggressive connection parameters during OTA:**
   - Connection interval: 7.5ms (minimum)
   - Slave latency: 0 (respond to every connection event)
   - Supervision timeout: 4000ms (tolerant of brief interference)
3. **Request connection parameter update** at OTA start, restore normal parameters after completion
4. **Even without resume**, the OTA partition is simply overwritten on restart -- no harm done, just wasted time

**Detection:**
- Force-stop mobile app mid-transfer and reconnect -- verify resume or clean restart behavior
- Test with device at maximum BLE range (10+ meters with obstacles)
- Count average transfer attempts before success in realistic conditions

**Phase:** Design into the protocol in Phase 1; implement in Phase 3 (BLE OTA). Adding resume later requires protocol changes that may break existing mobile app versions.

---

### Pitfall 10: Audio Stutters When BLE is Active on Core 1

**What goes wrong:** Audio task runs on Core 1 at priority 5 (`AUDIO_TASK_PRIORITY` in `app_config.h`). If NimBLE host task is also on Core 1 (recommended to avoid Core 0 deadlock -- see Pitfall 5), both tasks compete for CPU. During active BLE data transfer (especially OTA), NimBLE host processes many packets continuously, starving the audio task. The I2S DMA buffer underruns, causing audible pops, stutters, and gaps.

**Why it happens:** Both tasks at priority 5 get round-robin scheduling. BLE packet processing is bursty during OTA. The audio task's `taskYIELD()` (line 433 of `simple_audio_manager.cpp`) voluntarily yields CPU to NimBLE, but NimBLE may hold the CPU for several milliseconds processing a batch of packets.

**Prevention:**
1. **Give audio task higher priority than NimBLE host:** Audio at priority 6, NimBLE host at 5 (or use `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE` to adjust NimBLE host priority)
2. **Mute audio during OTA transfer.** Play an "update starting" audio clip, then silence all audio until OTA completes. There is no UX reason for journey audio feedback during firmware update. Call `stopAudio()` before starting OTA write
3. **Increase I2S DMA buffer depth** to tolerate longer audio task starvation (currently 100ms timeout for I2S writes at line 45)
4. **Monitor task stack usage:** `uxTaskGetStackHighWaterMark(audio->task_handle)` after BLE + audio coexistence testing

**Detection:**
- Run OTA transfer while playing an MP3 and listen for audio artifacts
- Log I2S write timeout occurrences during BLE activity
- Monitor audio task high-water mark under BLE load

**Phase:** Task priority decisions in Phase 1 (BLE Foundation). Integration testing in Phase 3 (BLE OTA).

---

### Pitfall 11: LittleFS Data Incompatibility Between Firmware Versions After Rollback

**What goes wrong:** With OTA, two firmware versions alternate between ota_0 and ota_1. Both share the same LittleFS data partition. If firmware V2 modifies data file paths (e.g., renames audio files in `app_config.h`), adds new config files, or changes data format, and the user rolls back to V1, the old firmware reads incompatible data and crashes or behaves incorrectly.

**Why it happens:** LittleFS is NOT OTA-updatable through `esp_ota` API. It is a shared resource. Developers modify constants like `AUDIO_FILE_IGN_ON` in V2 without considering V1 backward compatibility. After rollback, V1 tries to open the old filename which no longer exists.

**Prevention:**
1. **Version the data format:** Write a `data_version.txt` file to LittleFS. Each firmware checks version on boot and migrates data forward if needed
2. **Never rename or remove existing audio files.** Only add new ones. If file paths must change, keep old files as aliases
3. **Use NVS for settings** (version-independent key-value store), LittleFS only for immutable assets (audio, images)
4. **Test the rollback path:** Flash V2 via OTA, then force rollback to V1, verify V1 operates correctly
5. **Audio files are unlikely to change,** but if `AUDIO_FILE_*` constants change paths between versions, both versions must reference the same physical files

**Detection:**
- Automated test: OTA to V2, rollback to V1, verify all audio files play and all LittleFS reads succeed
- Compare `app_config.h` audio constants between firmware versions before release

**Phase:** Design data compatibility rules in Phase 1. Enforce in Phase 3 (BLE OTA) before first production OTA release.

---

### Pitfall 12: NimBLE Advertising Not Restarting After Disconnect

**What goes wrong:** Mobile phone disconnects from device. Device should restart advertising so phone can reconnect. But NimBLE does NOT auto-restart advertising after disconnect. The device becomes invisible to BLE scans.

**Why it happens:** This is a well-known NimBLE behavior. The stack requires the application to explicitly restart advertising in the `BLE_GAP_EVENT_DISCONNECT` handler. Developers test with a single connect/disconnect cycle and it works because they manually trigger advertising. In production, the automatic flow fails.

**Prevention:**
1. In `BLE_GAP_EVENT_DISCONNECT` handler, always call `ble_gap_adv_start()` to restart advertising
2. Implement advertising state machine:
   - Boot: advertising based on user preference (auto or manual)
   - Connected: advertising stops (single connection device)
   - Disconnected: restart advertising automatically (with configurable timeout)
3. Test: connect -> disconnect -> wait 5s -> verify device appears in scan -> reconnect

**Detection:**
- After disconnect, scan for the device from another phone -- if not visible, advertising restart is missing
- Add ESP_LOGI in disconnect handler to confirm advertising restart

**Phase:** Phase 1 (BLE Foundation). Must be in the BLE event handler template from the start.

---

## Minor Pitfalls

Issues that cause annoyance or minor rework but are easily fixable.

---

### Pitfall 13: LVGL Widgets Compiled Out -- Blank Screens for New UI

**What goes wrong:** The current `lv_conf.h` disables many widgets: `LV_USE_ARC 0`, `LV_USE_SWITCH 0`, `LV_USE_TEXTAREA 0`, `LV_USE_SLIDER 0`, `LV_USE_SPINNER 0`, `LV_USE_CHECKBOX 0`, `LV_USE_DROPDOWN 0`. New screens for BLE settings and OTA progress will need some of these. Using a disabled widget causes a compile error (if lucky) or runtime crash (if the header exists but code is compiled out).

**Prevention:**
1. Enable needed widgets in `lv_conf.h` BEFORE implementing new screens:
   - `LV_USE_BAR 1` -- already enabled, needed for OTA progress bar
   - `LV_USE_SPINNER 1` -- for "updating..." animation
   - `LV_USE_TEXTAREA 1` -- for BLE passkey entry (if using passkey pairing)
   - `LV_USE_SWITCH 1` -- for BLE enable/disable toggle
   - `LV_USE_ARC 1` -- for circular progress display (optional)
2. Each widget adds ~1-5 KB to firmware binary. Budget this into partition size calculations
3. Also enable development asserts that are currently disabled: `LV_USE_ASSERT_NULL 1`, `LV_USE_ASSERT_MALLOC 1`

**Phase:** Phase 2 (Multi-Screen Navigation) when defining screen designs.

---

### Pitfall 14: BLE Device Name Not Unique Across Multiple Devices

**What goes wrong:** NimBLE defaults to device name "nimble". When multiple ESP32-S3 devices are nearby (e.g., fleet vehicles in a depot), users cannot distinguish them in the mobile app BLE scan list.

**Prevention:**
1. Set unique device name incorporating MAC address: `ble_svc_gap_device_name_set("TecJornada-XXXX")` where XXXX is the last 4 hex digits of the BLE MAC
2. Include device serial number in advertising manufacturer-specific data
3. Include firmware version in the Device Information Service

**Phase:** Phase 1 (BLE Foundation).

---

### Pitfall 15: iOS GATT Cache Invalidation After Characteristic Changes

**What goes wrong:** iOS aggressively caches the GATT database. If the developer adds or removes GATT characteristics between firmware versions, iOS continues using the cached (stale) database. New characteristics are invisible; removed ones cause errors.

**Prevention:**
1. Increment BLE service changed indication when GATT layout changes
2. OR change the device name slightly to force iOS to re-discover services
3. Document in mobile app: users may need to "Forget This Device" in iOS Bluetooth settings after firmware updates that change GATT layout
4. Design the GATT service structure to be stable from V1 -- add reserve characteristics with empty handlers rather than adding new ones later

**Phase:** Phase 1 (BLE Foundation) during GATT service design.

---

### Pitfall 16: OTA Screen Not Shown Because Modal Popup Is Active

**What goes wrong:** OTA command arrives via BLE. The screen manager should navigate to OTA progress screen. But a modal popup is currently active (e.g., journey confirmation dialog from `ButtonManager::showPopup()`), blocking navigation. OTA data starts arriving but the user sees no progress indication.

**Prevention:**
1. Implement `forceNavigateTo()` in the screen manager for critical transitions (OTA, error states)
2. `forceNavigateTo()` closes any active popup first (`closePopup()`) then navigates
3. OTA start command should always force-navigate regardless of current UI state
4. The OTA screen should block all other navigation until OTA completes or is cancelled

**Phase:** Phase 2 (Multi-Screen Navigation) when implementing the navigation system.

---

### Pitfall 17: Config Sync Race Between Local Touch UI and BLE Remote Control

**What goes wrong:** User changes volume on the device touchscreen while the mobile app simultaneously sends a volume change via BLE. Final state is inconsistent -- device shows one volume, app shows another.

**Prevention:**
1. **Last-write-wins with notification:** after any config change (local or BLE), notify the other side
2. BLE write handler updates local state and sends BLE notification to the connected app
3. Local UI change updates local state and sends BLE notification if connected
4. Both sides converge on the latest value within one BLE connection interval

**Phase:** Phase 1 (BLE Foundation) protocol design.

---

## Phase-Specific Warnings Summary

| Phase | Likely Pitfall | Severity | Mitigation |
|---|---|---|---|
| **Phase 1: BLE Foundation** | RAM starvation (#1) | CRITICAL | Move display buffers to PSRAM, audit internal SRAM budget |
| **Phase 1: BLE Foundation** | NimBLE core pinning deadlock (#5) | CRITICAL | Pin NimBLE host to Core 1, implement BLE->UI event queue |
| **Phase 1: BLE Foundation** | Partition table migration (#3) | CRITICAL | Design OTA layout, plan one-time USB reflash for all devices |
| **Phase 1: BLE Foundation** | Security pairing config (#8) | MODERATE | LESC-only, "Just Works" pairing, test on diverse devices |
| **Phase 1: BLE Foundation** | Advertising restart (#12) | MODERATE | Always restart advertising in disconnect handler |
| **Phase 2: Multi-Screen Nav** | Screen memory leaks (#2) | CRITICAL | Implement screen lifecycle with auto-delete on transition |
| **Phase 2: Multi-Screen Nav** | Animation crash on BLE events (#7) | MODERATE | Navigation state machine with transition guard |
| **Phase 2: Multi-Screen Nav** | Missing LVGL widgets (#13) | MINOR | Enable needed widgets in lv_conf.h before implementation |
| **Phase 2: Multi-Screen Nav** | Force-navigate for OTA (#16) | MINOR | Implement forceNavigateTo() for critical screens |
| **Phase 3: BLE OTA** | Transfer corruption (#4) | CRITICAL | SHA-256 verification, app rollback, signed firmware |
| **Phase 3: BLE OTA** | MTU not negotiated (#6) | MODERATE | MTU 517, DLE, Write Without Response, fast interval |
| **Phase 3: BLE OTA** | Connection drop / no resume (#9) | MODERATE | Design resumable protocol, checkpoint offsets in NVS |
| **Phase 3: BLE OTA** | Audio stutter during OTA (#10) | MODERATE | Mute audio during OTA, or raise audio task priority |
| **Phase 3: BLE OTA** | Data format incompatibility (#11) | MODERATE | Version LittleFS data, test rollback scenarios |

---

## Memory Budget Estimate

Based on codebase analysis and ESP32-S3 constraints:

| Resource | Current (est.) | After BLE+OTA (est.) | Hard Limit |
|---|---|---|---|
| Internal SRAM (total usable) | ~200 KB used | ~280-310 KB used | ~320 KB |
| Internal SRAM (free target) | ~120 KB | >30 KB (minimum) | -- |
| PSRAM | ~300 KB (display buf) | ~350 KB | 8 MB available |
| Flash - app partition | ~2.5 MB binary | ~3.0-3.2 MB | 3.5 MB (proposed OTA) |
| Flash - LittleFS data | 408 KB actual | ~500 KB | 1 MB partition |
| Tasks on Core 0 | system_task(8KB), ignicao_task(4KB) | + NimBLE controller | -- |
| Tasks on Core 1 | audio_task(32KB) | + NimBLE host(4-8KB) | -- |

**Tightest constraint:** Internal SRAM. PSRAM migration of display buffers is non-negotiable before BLE integration.

---

## Testing Matrix for Pitfall Validation

| Test | Validates | Execution |
|---|---|---|
| Boot with BLE enabled, log heap sizes | Pitfall 1 | Add heap logging at 5 checkpoints, compare against budget |
| Navigate all screens 50 times, monitor heap | Pitfall 2 | Automated screen cycling with ESP heap logging |
| Full OTA cycle: ota_0 -> ota_1 -> ota_0 | Pitfalls 3, 4 | USB flash OTA-enabled FW, then BLE OTA twice |
| Corrupt 1 byte in OTA transfer mid-stream | Pitfall 4 | Mock corrupt chunk in test firmware, verify rejection |
| Rapid BLE connect/disconnect during screen animation | Pitfalls 5, 7 | nRF Connect: toggle connection 20 times in 30 seconds |
| Force-stop mobile app at 50% and 90% of OTA | Pitfall 9 | Kill app, reconnect, verify resume or clean restart |
| Play audio during active OTA transfer | Pitfall 10 | Start OTA while MP3 is playing, check for audio artifacts |
| Flash V2 via OTA, force rollback to V1 | Pitfall 11 | OTA V2, then reboot into V1, verify all audio plays |
| Pair with 5+ different Android devices | Pitfall 8 | Budget phone, flagship, Android 8-14 |

---

## Sources

- ESP-IDF documentation for NimBLE configuration (training data, ESP-IDF 5.x) -- MEDIUM confidence
- LVGL 8.x documentation for screen management and memory -- HIGH confidence (well-documented behavior)
- ESP32-S3 Technical Reference Manual for memory map and DMA -- MEDIUM confidence
- Codebase analysis: `sdkconfig.esp32s3_idf`, `sdkconfig.defaults`, `partitions.csv`, `lv_conf.h`, `app_config.h`, `button_manager.cpp` (1203 lines), `simple_audio_manager.cpp` (651 lines), `main.cpp` (237 lines), `jornada_keyboard.h` -- HIGH confidence
- Note: WebSearch was unavailable. All NimBLE-specific claims (memory usage, configuration options, default behaviors) should be verified against current ESP-IDF 5.3.1 documentation and NimBLE source before implementation.
