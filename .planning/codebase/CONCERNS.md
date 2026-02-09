# Codebase Concerns

**Analysis Date:** 2026-02-09

## Tech Debt

**Large Monolithic Files:**
- Issue: `src/button_manager.cpp` (1205 lines), `src/jornada_keyboard.cpp` (1064 lines), `src/lv_port.c` (622 lines) exceed recommended max of ~800 lines. This creates high cognitive load and makes maintenance/debugging harder.
- Files: `src/button_manager.cpp`, `src/jornada_keyboard.cpp`, `src/lv_port.c`, `src/esp_bsp.c` (539 lines), `src/numpad_example.cpp` (574 lines)
- Impact: Difficult to navigate, test, and refactor. Changes in one area risk breaking others in same file.
- Fix approach: Extract related functionality into separate modules (e.g., grid management into separate file from button creation, separate touch handling from LCD driver initialization).

**Legacy Code Not Yet Removed:**
- Issue: `src/main_old.cpp.bak` remains in codebase. Backup files clutter the repository and create confusion about what's active.
- Files: `src/main_old.cpp.bak`
- Impact: Potential confusion during refactoring; adds noise to codebase history.
- Fix approach: Delete backup files after confirming main.cpp is stable. Use git history instead of keeping .bak files.

**Mixed C/C++ Codebase Without Clear Separation:**
- Issue: Codebase mixes C (esp_*.c, lv_port.c) and C++ (.cpp files) with `extern "C"` blocks scattered throughout. Services use C++ objects (new/delete, std::vector) while HAL drivers are C.
- Files: All service files use C++ (`src/services/ignicao/ignicao_service.cpp`, `src/services/jornada/jornada_service.cpp`, `src/button_manager.cpp`) while `src/esp_bsp.c`, `src/esp_lcd_*.c` are pure C.
- Impact: Inconsistent memory management styles (malloc/free vs new/delete), increases risk of leaks, makes component integration harder.
- Fix approach: Establish clear boundary: C for HAL layer (drivers), C++ for services/UI. Consider wrapping C drivers in C++ classes for consistency.

## Memory Management Concerns

**Possible Memory Leak in Audio Manager Initialization:**
- Issue: `src/simple_audio_manager.cpp` allocates buffers in `audio_alloc_buffers()` via `heap_caps_malloc()`, but error paths may not clean up properly if allocation partially succeeds.
- Files: `src/simple_audio_manager.cpp` (lines 200-235)
- Problem: If `pcm_buffer` allocation fails, `mp3_buffer` is already allocated but not freed before returning error. `audio_free()` only handles one pointer per call.
- Impact: Memory leak of several KB when audio initialization partially fails. With 8MB flash and PSRAM constraints, this reduces available memory for other tasks.
- Fix approach: Implement compound cleanup that frees all allocated buffers atomically. Use a structure to track all three buffers and free all on error.

**Singleton Pattern Without Proper Cleanup:**
- Issue: Multiple singleton services (ButtonManager, IgnicaoService, JornadaService, Theme) use static pointers with new/delete, but destructors may not be called if destroyInstance() is never invoked.
- Files: `src/button_manager.cpp` (line 100), `src/services/ignicao/ignicao_service.cpp` (line 62), `src/services/jornada/jornada_service.cpp` (line 62), `src/ui/common/theme.cpp`
- Impact: If services are created but never destroyed, malloc'd memory is never freed. On embedded systems with limited RAM, this permanently reduces heap.
- Fix approach: Add startup/shutdown lifecycle management in app_init that ensures destroyInstance() is called for all services at shutdown.

**Heap Fragmentation Risk:**
- Issue: Multiple allocation sizes in different tasks (8KB audio buffer, variable-sized UI objects, touch buffers) can cause heap fragmentation. LVGL frequently allocates/deallocates for animations and state changes.
- Files: `src/simple_audio_manager.cpp` (8KB buffer, mp3dec allocation), `src/button_manager.cpp` (button objects), `src/lv_port.c` (display buffers)
- Impact: Over time, available contiguous free memory decreases even if total free memory is high. May cause allocation failures for large buffers later.
- Fix approach: Consider using memory pools for fixed-size allocations (e.g., audio buffers, button objects). Monitor fragmentation with heap_caps_get_info() periodically.

**PSRAM Not Fully Utilized:**
- Issue: Configuration allocates display buffers with `heap_caps_malloc()` but doesn't explicitly prefer PSRAM for large buffers. Audio buffers use `MALLOC_CAP_INTERNAL` only.
- Files: `src/lv_port.c` (lines 207-219), `src/simple_audio_manager.cpp` (lines 213-231)
- Impact: Large display/audio buffers consume internal RAM (precious resource on ESP32-S3) instead of using available PSRAM (8MB external).
- Fix approach: Modify allocation flags to prefer PSRAM for >32KB allocations. Display buffers should be: `MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA`.

## Thread Safety Concerns

**Race Condition: Uninitialized Global State in Audio Manager:**
- Issue: `isPlayingAudio` and `audioMutex` are global variables exposed externally (`extern bool isPlayingAudio`), but they're modified by audio_task (Core 1) and read by main (Core 0) without synchronization.
- Files: `src/simple_audio_manager.cpp` (lines 83-84)
- Problem: Core 0 may read `isPlayingAudio` without acquiring the mutex. Between checking and using the value, Core 1 may change it.
- Impact: Race condition in audio state machine. UI may show "playing" when audio stopped, or vice versa.
- Fix approach: Always access `isPlayingAudio` through mutex-protected functions. Remove extern variable. Provide getter function that acquires mutex.

**Display Lock Timeout Too Short:**
- Issue: Most `bsp_display_lock()` calls use 100ms timeout, but if LVGL is busy rendering complex scenes, lock acquisition may fail.
- Files: `src/button_manager.cpp` (multiple calls with 100ms), `src/jornada_keyboard.cpp` (100ms), `src/simple_ui.cpp` (100ms), one call uses 0ms (nonblocking).
- Problem: Line 217 in `src/button_manager.cpp` uses 200ms but others use 100ms. Inconsistent policy creates silent failures.
- Impact: UI updates silently fail when display is locked. Status bar won't update, button creation fails, state changes don't reflect.
- Fix approach: Standardize on portMAX_DELAY for critical operations, or implement exponential backoff retry. Use 100ms only for non-critical updates.

**Potential Deadlock in Service Initialization:**
- Issue: If `init()` is called on a service and mutex creation fails, `initialized` flag is never set but future calls still return early.
- Files: `src/services/jornada/jornada_service.cpp` (lines 78-99), `src/services/ignicao/ignicao_service.cpp` (lines 73-96)
- Problem: If `xSemaphoreCreateMutex()` fails, the service is partially initialized. Next init() call returns early without creating mutex. Operations fail silently.
- Impact: Service appears initialized but has no mutex protection. Concurrent access to `motoristas[]` or `currentStatus` causes corruption.
- Fix approach: In init(), if critical initialization fails, set `initialized = false` explicitly and log critical error. Require explicit retry.

**Audio Task Stack Size May Be Insufficient:**
- Issue: Audio task uses 32KB stack (`AUDIO_TASK_STACK_SIZE = 32 * 1024`), but MP3 decoding with minimp3 may need variable stack depending on audio complexity.
- Files: `src/simple_audio_manager.cpp` (line 39), `include/config/app_config.h` (line 104)
- Impact: Stack overflow during MP3 decoding can corrupt heap or cause system crash. No stack overflow detection in place.
- Fix approach: Monitor with uxTaskGetStackHighWaterMark() to determine actual peak usage. Consider 48KB minimum for safety margin.

## Display/UI Concerns

**LVGL Object Validity Not Checked Before Use:**
- Issue: Many operations fetch LVGL objects and use them without re-validating after releasing the lock.
- Files: `src/button_manager.cpp` (getButton() returns pointer, may be deleted later), `src/jornada_keyboard.cpp` (checks with `lv_obj_is_valid()` after re-acquiring lock, but not everywhere)
- Problem: Between lock release and object use, LVGL may delete the object. Dereference causes segfault.
- Impact: Random crashes during UI state transitions, especially during rapid button presses.
- Fix approach: Require all LVGL operations to happen within lock scope. Don't return pointers to UI objects outside lock context. Use object IDs instead.

**Animation Callbacks Not Guarded:**
- Issue: `src/jornada_keyboard.cpp` (lines 437-438) registers animation deleted callback but relies on pointer validity.
- Files: `src/jornada_keyboard.cpp` (line 438: `lv_anim_set_deleted_cb()`)
- Problem: Callback receives lv_anim_t* which may access invalid data if screen destroyed during animation.
- Impact: Crashes if screen changes while animation running.
- Fix approach: Store screen reference in callback context and validate before access.

**Status Bar Update Timer May Outlive Manager:**
- Issue: `src/button_manager.cpp` creates LVGL timers (`statusUpdateTimer`) that reference `statusBar` pointer, but timer is deleted in destructor. If manager destroyed while timer firing, use-after-free.
- Files: `src/button_manager.cpp` (lines 49-50, 81-83)
- Impact: Crash if ButtonManager destroyed while timer callback executing.
- Fix approach: Stop timer before destruction. Or use weak references/validation in timer callbacks.

## File System Concerns

**No Error Checking on File Open Failures:**
- Issue: `src/lvgl_fs_driver.cpp` logs warnings when files fail to open, but doesn't prevent repeated attempts or handle missing files gracefully.
- Files: `src/lvgl_fs_driver.cpp` (lines 43-46)
- Problem: If PNG splash or MP3 files missing, system logs warning but continues. PNG decoder may crash trying to read null pointer. MP3 playback skips silently.
- Impact: Unreliable startup or missing audio without clear user feedback.
- Fix approach: Add validation at startup to ensure all required audio/image files exist. Implement fallback behavior (e.g., play silence, show default splash).

**LittleFS Filesystem Not Validated on Mount:**
- Issue: `src/core/app_init.cpp` (line 99) catches errors but doesn't validate partition integrity or available space.
- Files: `src/core/app_init.cpp` (lines 43-62)
- Problem: Corrupted LittleFS partition won't format if `FS_FORMAT_IF_FAILED = false`. System hangs during startup.
- Impact: Hardware becomes unusable. No recovery without serial connection and reflashing.
- Fix approach: Implement automatic format-on-corruption with user notification. Or require explicit recovery procedure.

## Scaling & Performance Concerns

**Hardcoded Limits Block Expansion:**
- Issue: Multiple hardcoded limits prevent scaling:
  - `MAX_MOTORISTAS = 3` in `include/config/app_config.h` (line 74)
  - `GRID_COLS = 4, GRID_ROWS = 3` (lines 51-52)
  - `AUDIO_QUEUE_SIZE = 4` (line 108)
- Files: `include/config/app_config.h`
- Impact: Can't add more drivers, expand button grid, or queue more audio without recompile. Arrays are stack-allocated.
- Fix approach: Convert fixed arrays to vectors with dynamic sizing. Add runtime configuration option.

**Display Buffer Allocation Fixed at Build Time:**
- Issue: `DISPLAY_BUFFER_SIZE` calculated as `DISPLAY_WIDTH * DISPLAY_HEIGHT` at compile time (line 45). Double buffering not configured.
- Files: `include/config/app_config.h` (line 45), `src/lv_port.c` (line 207)
- Impact: Single-buffer rendering causes flicker. No tearing detection despite TE pin available.
- Fix approach: Implement double-buffering with tear detection. Configure based on available PSRAM.

**No Performance Monitoring:**
- Issue: System has no frame-rate monitoring, memory usage tracking, or CPU load measurement.
- Impact: Difficult to diagnose performance degradation. Can't identify which component consumes resources.
- Fix approach: Implement periodic heap/stack monitoring logged to serial. Add FPS counter to status bar.

## Error Handling Gaps

**Ignition Service Silent Failures:**
- Issue: `src/services/ignicao/ignicao_service.cpp` init() (line 94) returns void. Failures logged but not propagated.
- Files: `src/services/ignicao/ignicao_service.cpp` (lines 78-96)
- Problem: Main code doesn't verify if ignition service initialized. GPIO18 polling may not start.
- Impact: Ignition state not detected. All ignition-based logic fails silently.
- Fix approach: Make init() return bool. Check return value and retry or enter safe mode.

**Button Creation Retry Logic Only Used Sporadically:**
- Issue: `src/button_manager.cpp` has retry logic (lines 237-254) but only used internally. Callers don't know if creation succeeded.
- Files: `src/button_manager.cpp`
- Problem: Button creation failure returns -1 but callers may not check. UI logic assumes button exists.
- Impact: Clicks on non-existent buttons cause callback errors.
- Fix approach: Implement proper error propagation to all callers. Log critical failures. Implement fallback UI.

**No Null Check After Singleton getInstance():**
- Issue: Callers of `getInstance()` don't check for null in case of allocation failure.
- Files: Every service getter call throughout codebase
- Problem: If new JornadaService() fails to allocate, returns nullptr. Callers dereference without checking.
- Impact: Segfault if memory exhausted during service creation.
- Fix approach: Make getInstance() assert/exit on failure. Or implement resource pool pre-allocation at startup.

## Security Considerations

**No Input Validation on Motorista Data:**
- Issue: Jornada service accepts motorista data without validation.
- Files: `src/services/jornada/jornada_service.cpp` (lines 150+)
- Problem: No bounds checking on name length, ID ranges, or state transitions. External code could trigger invalid states.
- Impact: Buffer overflows (e.g., nome > 32 chars), invalid state machine transitions.
- Fix approach: Implement Zod-like schema validation or asserts on state transitions. Validate all external input.

**No Protection Against Rapid UI Clicks:**
- Issue: Debounce logic in `src/button_manager.cpp` (line 27: `BUTTON_DEBOUNCE_MS = 300`) but implemented as global static variables, not per-button.
- Files: `src/button_manager.cpp` (lines 29-30)
- Problem: Spamming clicks on same button can queue multiple events before debounce triggers.
- Impact: Multiple audio plays queued, multiple state changes triggered.
- Fix approach: Implement per-button debounce with proper reset logic.

## Integration & Compatibility Issues

**Bluetooth Support Gap:**
- Issue: Hardware has Bluetooth capability (ESP32-S3 native) but no BLE implementation. CLAUDE.md mentions future BLE support.
- Impact: No wireless control, no remote sync. System is isolated from other devices.
- Fix approach: Plan BLE integration for future phase. Design APIs to support remote commands.

**No OTA (Over-The-Air) Update Support:**
- Issue: Partitions configured for single app, no OTA partition. Every update requires serial connection.
- Files: `include/config/app_config.h` comments, `platformio.ini` (comment: "No OTA configured")
- Impact: Can't update fleet devices remotely. Firmware fixes require physical access.
- Fix approach: Reconfigure partitions for OTA. Implement secure update mechanism.

## Test Coverage Gaps

**No Automated Testing:**
- Issue: Project has no test framework (mentioned in CLAUDE.md: "no test framework configured"). All verification is manual flashing and hardware testing.
- Files: No test files found in codebase
- Impact: Refactoring risks breaking functionality. Regressions only caught in field.
- Fix approach: Implement CMake test harness for unit testing non-LVGL components (services, utils). Use integration tests for UI.

**No Stress Testing:**
- Issue: Audio queue (size 4) and button grid never tested under rapid/concurrent operations.
- Impact: Unknown behavior under stress. Race conditions may only manifest with specific timing.
- Fix approach: Create stress test that plays audio while rapidly pressing buttons and changing screens.

**Critical Path Not Validated:**
- Issue: No end-to-end test for ignition detection → audio playback → status update flow.
- Impact: Changes to any component break entire workflow without verification.
- Fix approach: Implement integration test that simulates ignition toggle and verifies audio queued and status updated.

---

*Concerns audit: 2026-02-09*
