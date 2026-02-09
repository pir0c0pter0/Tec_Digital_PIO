# Phase 1: Foundation - Research

**Researched:** 2026-02-09
**Domain:** ESP32-S3 embedded firmware -- partition table, LVGL screen management, NVS persistence
**Confidence:** HIGH

## Summary

Phase 1 transforms the current single-app firmware into an OTA-ready, multi-screen architecture with persistent storage. The codebase currently uses a monolithic `ButtonManager` (1205 lines) that owns both the screen and status bar, with a simplistic `ScreenManager` that swaps button grids in/out of a single LVGL screen. The new architecture requires: (1) a new partition table replacing the factory-only layout with dual OTA slots, (2) a proper screen manager with stack-based navigation and animated transitions via `lv_scr_load_anim()`, (3) a persistent StatusBar on `lv_layer_top()` that survives screen transitions, and (4) an NVS persistence service on a dedicated partition for settings and journey state.

The existing `IScreen` and `IScreenManager` interfaces in `include/interfaces/i_screen.h` are well-designed and already define `create()`, `destroy()`, `onEnter()`, `onExit()`, `update()`, `navigateTo()`, `goBack()`, and `registerScreen()`. These interfaces must be implemented by the new ScreenManager and screen classes. The current `ScreenManager` class in `jornada_keyboard.h` (which does NOT implement `IScreenManager`) will be replaced.

**Primary recommendation:** Implement the four plans sequentially -- partition table first (irreversible, validates hardware), then screen manager framework with StatusBar, then screen extraction from `button_manager.cpp`, and finally NVS persistence. Each plan must preserve all existing v1.x functionality at every step.

## Standard Stack

### Core (already in project)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ESP-IDF | 5.3.1 (espressif32@6.9.0) | Framework | PlatformIO board platform, already configured |
| LVGL | 8.4.0 (lib/lvgl/) | GUI toolkit | Local copy in lib/, configured via lv_conf.h |
| FreeRTOS | Bundled with ESP-IDF | RTOS | Core scheduling, mutexes, tasks |
| LittleFS | joltwallet__littlefs | Filesystem | Audio/image assets on spiffs partition |
| minimp3 | Header-only (include/) | Audio decoder | MP3 decoding for I2S playback |

### ESP-IDF Components (needed, already available)
| Component | Header | Purpose | When to Use |
|-----------|--------|---------|-------------|
| NVS Flash | `nvs_flash.h` | Non-volatile storage | Settings + journey state persistence |
| ESP Partition | `esp_partition.h` | Partition access | Custom NVS partition initialization |
| ESP OTA Ops | `esp_ota_ops.h` | OTA management | Boot slot selection (future phases) |
| ESP Log | `esp_log.h` | Logging | Already used throughout |

### No New Dependencies Required
This phase uses only ESP-IDF built-in components. No new libraries need to be added to `platformio.ini` or `CMakeLists.txt` REQUIRES.

## Architecture Patterns

### Recommended Project Structure (additions for Phase 1)
```
include/
  config/
    app_config.h          # Add NVS/OTA constants
  interfaces/
    i_screen.h            # ALREADY EXISTS - use as-is
    i_nvs.h               # NEW: NVS service interface
  services/
    nvs/
      nvs_manager.h       # NEW: NVS manager header
  ui/
    screens/
      i_screen_impl.h     # NEW: Base screen implementation helpers
      jornada_screen.h    # NEW: Extracted from button_manager.cpp
      numpad_screen.h     # NEW: Extracted from numpad_example.cpp
    screen_manager.h      # NEW: Replaces current ScreenManager
    status_bar.h          # NEW: Persistent status bar

src/
  services/
    nvs/
      nvs_manager.cpp     # NEW: NVS implementation
  ui/
    screens/
      jornada_screen.cpp  # NEW: Extracted + adapted
      numpad_screen.cpp   # NEW: Extracted + adapted
    screen_manager.cpp    # NEW: Stack-based navigation
    status_bar.cpp        # NEW: lv_layer_top() status bar
```

### Pattern 1: Screen Lifecycle (implements existing IScreen)
**What:** Each screen implements `IScreen` from `i_screen.h` with full lifecycle management.
**When to use:** Every navigable screen in the application.
**Key insight:** The interface ALREADY defines `create()`, `destroy()`, `isCreated()`, `update()`, `onEnter()`, `onExit()`. Implementation must match.

```cpp
// IScreen is already defined in include/interfaces/i_screen.h
// Implementation pattern:
class JornadaScreen : public IScreen {
private:
    lv_obj_t* screen_;
    bool created_;
    // ... screen-specific state

public:
    ScreenType getType() const override { return ScreenType::JORNADA; }

    void create() override {
        screen_ = lv_obj_create(NULL);  // Create detached screen
        // Build UI elements on screen_
        created_ = true;
    }

    void onEnter() override {
        // Called AFTER screen becomes visible
        // Start timers, refresh data
    }

    void onExit() override {
        // Called BEFORE screen loses visibility
        // Stop timers, save state
    }

    void destroy() override {
        if (screen_) {
            lv_obj_del(screen_);
            screen_ = nullptr;
        }
        created_ = false;
    }

    void update() override {
        // Periodic update from main loop
    }

    bool isCreated() const override { return created_; }
};
```

### Pattern 2: Stack-Based Screen Manager (implements existing IScreenManager)
**What:** ScreenManager maintains a navigation stack, uses `lv_scr_load_anim()` for transitions.
**When to use:** All screen navigation.
**Critical detail:** The existing `IScreenManager` already defines `navigateTo()`, `goBack()`, `registerScreen()`, `update()`, `getCurrentScreen()`.

```cpp
// Source: LVGL 8 docs - lv_scr_load_anim()
class ScreenManagerImpl : public IScreenManager {
private:
    std::vector<ScreenType> navStack_;
    IScreen* screens_[static_cast<int>(ScreenType::MAX_SCREENS)];
    ScreenType currentScreen_;

public:
    void navigateTo(ScreenType type) override {
        IScreen* oldScreen = screens_[static_cast<int>(currentScreen_)];
        IScreen* newScreen = screens_[static_cast<int>(type)];

        if (oldScreen) oldScreen->onExit();

        navStack_.push_back(currentScreen_);

        if (!newScreen->isCreated()) newScreen->create();
        newScreen->onEnter();

        // Animate transition: slide left (push forward)
        lv_scr_load_anim(newScreen->getLvScreen(),
                         LV_SCR_LOAD_ANIM_MOVE_LEFT,
                         250,    // 250ms duration
                         0,      // no delay
                         false); // do NOT auto_del -- we manage lifecycle

        currentScreen_ = type;
    }

    bool goBack() override {
        if (navStack_.empty()) return false;
        ScreenType prev = navStack_.back();
        navStack_.pop_back();

        IScreen* oldScreen = screens_[static_cast<int>(currentScreen_)];
        IScreen* newScreen = screens_[static_cast<int>(prev)];

        if (oldScreen) {
            oldScreen->onExit();
            oldScreen->destroy();  // Free memory on pop
        }

        if (!newScreen->isCreated()) newScreen->create();
        newScreen->onEnter();

        // Animate transition: slide right (pop back)
        lv_scr_load_anim(newScreen->getLvScreen(),
                         LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                         250, 0, false);

        currentScreen_ = prev;
        return true;
    }
};
```

### Pattern 3: Persistent StatusBar on lv_layer_top()
**What:** StatusBar lives on `lv_layer_top()`, visible across all screen transitions.
**When to use:** Ignition state, timers, menu/back buttons.
**Source:** LVGL 8 documentation confirms `lv_layer_top()` persists across `lv_scr_load_anim()` transitions.

```cpp
// Source: LVGL 8 display docs
class StatusBar {
private:
    lv_obj_t* container_;  // Parent: lv_layer_top()

public:
    void create() {
        container_ = lv_obj_create(lv_layer_top());
        lv_obj_set_size(container_, 480, 40);
        lv_obj_align(container_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        // ... add ignition indicator, timers, menu button, back button
    }
};
```

### Pattern 4: NVS Manager with Custom Partition
**What:** Dedicated NVS partition for app data, separate from system NVS.
**When to use:** Persisting settings (volume, brightness) and journey state.
**Source:** ESP-IDF NVS documentation.

```cpp
// Source: ESP-IDF NVS Flash docs
#include "nvs_flash.h"

class NvsManager {
private:
    static constexpr const char* PARTITION_LABEL = "nvs_data";
    static constexpr const char* NS_SETTINGS = "settings";
    static constexpr const char* NS_JORNADA = "jornada";

public:
    bool init() {
        esp_err_t err = nvs_flash_init_partition(PARTITION_LABEL);
        if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase_partition(PARTITION_LABEL);
            err = nvs_flash_init_partition(PARTITION_LABEL);
        }
        return err == ESP_OK;
    }

    bool setU8(const char* ns, const char* key, uint8_t value) {
        nvs_handle_t handle;
        esp_err_t err = nvs_open_from_partition(PARTITION_LABEL, ns,
                                                 NVS_READWRITE, &handle);
        if (err != ESP_OK) return false;
        err = nvs_set_u8(handle, key, value);
        if (err == ESP_OK) nvs_commit(handle);
        nvs_close(handle);
        return err == ESP_OK;
    }

    // Journey state: use nvs_set_blob() for struct serialization
    bool saveJornadaState(int motoristId, const JornadaState& state) {
        nvs_handle_t handle;
        char key[16];
        snprintf(key, sizeof(key), "mot_%d", motoristId);
        esp_err_t err = nvs_open_from_partition(PARTITION_LABEL, NS_JORNADA,
                                                 NVS_READWRITE, &handle);
        if (err != ESP_OK) return false;
        err = nvs_set_blob(handle, key, &state, sizeof(state));
        if (err == ESP_OK) nvs_commit(handle);
        nvs_close(handle);
        return err == ESP_OK;
    }
};
```

### Anti-Patterns to Avoid
- **Using `auto_del=true` in `lv_scr_load_anim()`:** Known bugs in LVGL 8.x cause crashes and memory leaks, especially with 0ms animation time. Always use `auto_del=false` and manage screen lifecycle manually via `lv_obj_del()`.
- **Creating LVGL objects from Core 1:** All LVGL calls MUST happen from Core 0 (system_task context). The current codebase correctly follows this pattern.
- **Mutating ButtonManager while extracting screens:** The existing `button_manager.cpp` must remain functional until JornadaScreen and NumpadScreen are fully verified. Extract, don't refactor in-place.
- **Calling `lv_scr_load_anim()` while previous animation is running:** Can cause crashes. Must wait for previous transition to complete or use `LV_SCR_LOAD_ANIM_NONE` to cancel.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| NVS key-value storage | Custom file-based settings | `nvs_flash.h` API | Wear leveling, atomic writes, power-loss safe |
| Screen transitions | Manual LVGL object animation | `lv_scr_load_anim()` | Built-in, handles input blocking during transition |
| Persistent UI layer | Recreating status bar per screen | `lv_layer_top()` | LVGL native display layer, survives screen loads |
| OTA partition management | Custom bootloader logic | `esp_ota_ops.h` (future) | ESP-IDF handles rollback, verification, boot selection |
| Struct serialization to NVS | Custom binary format | `nvs_set_blob()` / `nvs_get_blob()` | Direct struct storage for flat POD types |

**Key insight:** ESP-IDF and LVGL 8 already provide all the primitives needed. The work is in architecture (screen lifecycle, navigation stack) not in low-level implementations.

## Common Pitfalls

### Pitfall 1: Partition Table Change is Irreversible Over-the-Air
**What goes wrong:** Changing the partition table requires USB reflash. If devices are already deployed, they cannot be updated remotely.
**Why it happens:** The partition table is written at offset 0x8000 by the bootloader, not by the application.
**How to avoid:** Validate with `gen_esp32part.py` before flashing. Test on a development board first. Document the procedure for field device reflash.
**Warning signs:** `ESP_ERR_OTA_VALIDATE_FAILED` on boot, device stuck in bootloop.

### Pitfall 2: lv_scr_load_anim() Memory Leaks with auto_del
**What goes wrong:** Using `auto_del=true` causes dangling pointers and memory leaks, especially when animation time is 0ms or when rapidly switching screens.
**Why it happens:** LVGL deletes the old screen asynchronously after animation completes, but user code may still hold references. Known bug in LVGL 8.x.
**How to avoid:** Always use `auto_del=false`. Explicitly call `lv_obj_del()` on old screens in `destroy()` method, after the transition animation is confirmed complete.
**Warning signs:** Heap fragmentation warnings, increasing memory usage after multiple screen transitions, random crashes.

### Pitfall 3: StatusBar on lv_layer_top() Blocks Touch Events
**What goes wrong:** The `lv_layer_top()` overlay captures all touch events in its area, preventing buttons underneath from receiving clicks.
**Why it happens:** `lv_layer_top()` is above all screens in the Z-order. Any object on it with `LV_OBJ_FLAG_CLICKABLE` will intercept touches.
**How to avoid:** Make the StatusBar container non-clickable (`lv_obj_clear_flag(container, LV_OBJ_FLAG_CLICKABLE)`), but keep specific buttons (menu, back) clickable. Use `lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE)`.
**Warning signs:** Touch input stops working on the bottom 40px of screens.

### Pitfall 4: NVS Namespace Name 15-Character Limit
**What goes wrong:** Namespace names exceeding 15 characters silently fail or cause `ESP_ERR_NVS_INVALID_NAME`.
**Why it happens:** NVS internal key format uses fixed-size fields. Maximum is `NVS_KEY_NAME_MAX_SIZE - 1 = 15` characters.
**How to avoid:** Keep namespace names short: `"settings"` (8), `"jornada"` (7). Key names also limited to 15 characters.
**Warning signs:** `ESP_ERR_NVS_INVALID_NAME` return code.

### Pitfall 5: Blob Struct Padding and Alignment
**What goes wrong:** Saving a struct via `nvs_set_blob()` and reading it back produces garbage data if the struct has different padding on different compiler settings.
**Why it happens:** Compiler may add padding bytes between struct members. If the struct definition changes between firmware versions, old data becomes incompatible.
**How to avoid:** Use `__attribute__((packed))` on NVS-stored structs. Add a version field at the start of each struct. Validate version on read, reset to defaults if mismatched.
**Warning signs:** Corrupted journey state after firmware update.

### Pitfall 6: Extracting from button_manager.cpp Breaks Callbacks
**What goes wrong:** Moving jornada button logic out of ButtonManager breaks static callback functions that reference `ButtonManager::getInstance()`.
**Why it happens:** The current architecture tightly couples UI creation (in ButtonManager) with domain logic (in JornadaKeyboard). Both use `ButtonManager::getInstance()` for display lock/unlock.
**How to avoid:** Each extracted screen must own its own LVGL screen object and manage its own UI elements. The screen gets a reference to shared services (audio, ignition) via dependency injection, not global singletons.
**Warning signs:** Null pointer crashes, buttons not responding after extraction.

### Pitfall 7: Screen Navigation During Animation
**What goes wrong:** User taps a button during screen transition, causing `lv_scr_load_anim()` to be called while previous animation is still running.
**Why it happens:** LVGL 8 disables input during animation, but code-triggered navigation (not user input) is not blocked.
**How to avoid:** Add a `transitioning_` flag to ScreenManager. Block `navigateTo()` and `goBack()` calls while `transitioning_` is true. Clear the flag after animation duration (use `lv_timer_create` with the transition duration as the callback delay).
**Warning signs:** LVGL assert failures, visual glitches, double screen loads.

## Code Examples

### Partition Table CSV (validated for 16MB flash)
```csv
# Dual OTA partition table for ESP32-S3 16MB Flash
# Teclado de Jornada Digital v2.0
# Name,     Type, SubType, Offset,   Size,     Flags
nvs,        data, nvs,     0x9000,   0x4000,
otadata,    data, ota,     0xD000,   0x2000,
phy_init,   data, phy,     0xF000,   0x1000,
ota_0,      app,  ota_0,   0x10000,  0x300000,
ota_1,      app,  ota_1,   0x310000, 0x300000,
spiffs,     data, spiffs,  0x610000, 0x100000,
nvs_data,   data, nvs,     0x710000, 0x10000,
```

**Validation results:**
- All app partitions 64KB-aligned (0x10000, 0x310000)
- All data partitions 4KB-aligned
- otadata = 0x2000 (8KB, required by OTA)
- ota_0 = ota_1 = 3MB (matches current app0 size)
- spiffs = 1MB (unchanged for LittleFS assets)
- nvs_data = 64KB (dedicated app NVS)
- Total used: 7.125MB of 16MB (44.5% utilization)
- nvs reduced from 20KB to 16KB (still above 12KB minimum)

### lv_conf.h Widget Enablement
```c
// Widgets to enable (currently 0, need to be 1):
#define LV_USE_ARC        1   // Was 0 - needed for future settings screen
#define LV_USE_SLIDER     1   // Was 0 - needed for volume/brightness controls
#define LV_USE_METER      1   // Was 0 - needed for future dashboard

// Already enabled (no change needed):
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_MSGBOX     1
```

### NVS Journey State Struct
```cpp
// Packed struct for NVS blob storage
struct __attribute__((packed)) NvsJornadaState {
    uint8_t version;           // Struct version for migration
    uint8_t estadoAtual;       // Current EstadoJornada enum value
    uint32_t tempoInicio;      // millis() when state started
    uint32_t tempoTotalJornada;
    uint32_t tempoTotalManobra;
    uint32_t tempoTotalRefeicao;
    uint32_t tempoTotalEspera;
    uint32_t tempoTotalDescarga;
    uint32_t tempoTotalAbastecimento;
    bool ativo;                // Is motorist active
};

#define NVS_JORNADA_STATE_VERSION 1
```

### sdkconfig.defaults Additions for OTA
```
# OTA support
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_BOOTLOADER_FACTORY_RESET=n

# NVS encryption not needed (no WiFi credentials stored)
CONFIG_NVS_ENCRYPTION=n
```

## State of the Art

| Old Approach (current) | New Approach (Phase 1) | Impact |
|------------------------|------------------------|--------|
| Factory-only partition (no OTA) | Dual OTA partitions (ota_0 + ota_1) | Enables field firmware updates via BLE |
| Single LVGL screen with button swapping | Multiple LVGL screens with animated transitions | Clean navigation, proper memory lifecycle |
| StatusBar embedded in ButtonManager screen | StatusBar on `lv_layer_top()` | Persists across all screens without recreation |
| No persistent state | NVS on dedicated partition | Settings and journey state survive reboots |
| ScreenManager toggles 2 screens only | Stack-based navigation with push/pop | Scales to N screens (settings, diagnostics, etc.) |
| `ScreenManager` does not implement `IScreenManager` | New ScreenManager implements `IScreenManager` | Follows existing interface contract |

**Key existing assets to preserve:**
- `IScreen` interface in `i_screen.h` -- already has `create()`, `destroy()`, `onEnter()`, `onExit()`, `update()`
- `IScreenManager` interface in `i_screen.h` -- already has `navigateTo()`, `goBack()`, `registerScreen()`
- `ScreenType` enum -- already includes SPLASH, NUMPAD, JORNADA, SETTINGS
- All audio playback, ignition monitoring, LittleFS must continue working

## Critical Analysis of Current Code

### button_manager.cpp (1205 lines) -- Decomposition Analysis
The file contains:
1. **ButtonManager class** (lines 1-1100): Grid-based button system with retry logic, status bar, popups
2. **Global wrappers** (lines 1107-1205): `extern "C"` functions for main.cpp compatibility

**What must be extracted to JornadaScreen:**
- `createDefaultJornadaButtons()` (lines 1145-1165) -- button definitions
- All references to `JornadaKeyboard` and `onActionButtonClick`
- Popup system (lines 892-1076) if used by jornada

**What must be extracted to NumpadScreen:**
- The entire `NumpadExample` class (in numpad_example.cpp, 575 lines)
- Button layout definitions and callbacks

**What stays in a shared utility:**
- `formatTime()` (lines 1116-1131)
- `getStateColor()` (lines 1133-1143)
- `StatusBarData` struct

### jornada_keyboard.cpp (1065 lines) -- Contains Current ScreenManager
This file contains BOTH `JornadaKeyboard` AND `ScreenManager`. The current `ScreenManager` (lines 826-1065):
- Does NOT implement `IScreenManager`
- Has only `TELA_NUMPAD` and `TELA_JORNADA` (enum, not ScreenType)
- Swaps content by clearing/recreating buttons on the SAME screen
- Creates a floating switch button on `lv_scr_act()` (not on layer_top)

**The entire `ScreenManager` class in jornada_keyboard.h/cpp must be replaced** by a new implementation that:
1. Implements `IScreenManager` interface
2. Uses separate LVGL screens (not button swapping)
3. Uses navigation stack (push/pop)
4. Uses `lv_scr_load_anim()` for transitions

## Open Questions

1. **phy_init partition for BLE**
   - What we know: NimBLE is planned for BLE (Phase 3+). phy_init stores RF calibration data.
   - What's unclear: Whether NimBLE on ESP-IDF 5.3.1 requires phy_init partition or if it auto-calibrates
   - Recommendation: Keep phy_init partition (4KB cost is negligible). Safer to include it.

2. **Display Lock Pattern for Screen Transitions**
   - What we know: `bsp_display_lock(timeout)` / `bsp_display_unlock()` wraps all LVGL calls
   - What's unclear: Whether `lv_scr_load_anim()` needs to be called inside a display lock
   - Recommendation: Yes, call within display lock. The current codebase always locks before LVGL calls. Follow the same pattern.

3. **Screen Object Memory Budget**
   - What we know: ESP32-S3 has PSRAM (CONFIG_SPIRAM=y), LVGL uses custom malloc (stdlib)
   - What's unclear: Whether creating multiple LVGL screens simultaneously causes memory pressure
   - Recommendation: Use create-on-demand, destroy-on-pop pattern. Only keep the current screen and the home screen (NUMPAD) alive. Monitor with `heap_caps_get_free_size(MALLOC_CAP_DEFAULT)`.

## Sources

### Primary (HIGH confidence)
- ESP-IDF Partition Tables (ESP32-S3) v5.5.2: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/partition-tables.html
- ESP-IDF NVS Flash API v5.5.2: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html
- LVGL 8 Display/Layer Documentation: https://docs.lvgl.io/8/overview/display.html
- Existing codebase: `include/interfaces/i_screen.h` (IScreen, IScreenManager interfaces)
- Existing codebase: `include/config/app_config.h` (all system constants)
- Existing codebase: `partitions.csv` (current factory-only layout)

### Secondary (MEDIUM confidence)
- LVGL 8 Screen Animation Forum Discussions: https://forum.lvgl.io/t/lv-scr-load-anim-lv-obj-t-new-scr-lv-scr-load-anim-t-anim-type-uint32-t-time-uint32-t-delay-bool-auto-del/3815
- LVGL auto_del crash issue #3013: https://github.com/lvgl/lvgl/issues/3013
- LVGL memory leak issue #4212: https://github.com/lvgl/lvgl/issues/4212
- ESP32 Forum NVS blob storage patterns: https://www.esp32.com/viewtopic.php?t=32829

### Tertiary (LOW confidence)
- None -- all findings verified with official docs or codebase inspection

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- verified against existing `platformio.ini`, `CMakeLists.txt`, and ESP-IDF docs
- Architecture: HIGH -- based on existing `IScreen`/`IScreenManager` interfaces in codebase and LVGL 8 official docs
- Partition table: HIGH -- calculated and validated against ESP-IDF alignment rules, cross-checked with 16MB flash address space
- NVS patterns: HIGH -- verified against ESP-IDF NVS API docs with code examples
- Pitfalls: HIGH -- sourced from LVGL GitHub issues and codebase analysis of tight coupling
- Screen extraction risk: MEDIUM -- complexity depends on hidden dependencies in button_manager.cpp callbacks

**Research date:** 2026-02-09
**Valid until:** 2026-03-09 (30 days -- stable domain, no fast-moving dependencies)
