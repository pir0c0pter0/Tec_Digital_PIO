# Architecture

**Analysis Date:** 2026-02-09

## Pattern Overview

**Overall:** Layered Service-Oriented Architecture with dual-core task distribution

**Key Characteristics:**
- Clean separation between business logic (Services), presentation (UI), and hardware abstraction (HAL/Drivers)
- Interface-based design enabling swappable implementations
- Callback pattern for event decoupling (ignition, journey state changes)
- Thread-safe singleton services with FreeRTOS mutex protection
- Dual-core execution: Core 0 (UI/LVGL), Core 1 (Audio decoding)
- Centralized configuration in `include/config/app_config.h`

## Layers

**Application / Orchestration:**
- Purpose: Application entry point and main event loop coordination
- Location: `src/main.cpp`
- Contains: app_main(), system_task event loop (5ms LVGL ticker), ignition/journey callbacks
- Depends on: All services, UI components, utilities
- Used by: ESP-IDF as entry point via app_main()

**Services (Business Logic):**
- Purpose: Core domain logic for ignition monitoring, journey state management, and audio playback
- Location: `src/services/ignicao/`, `src/services/jornada/`, `src/services/audio/`
- Contains:
  - `IgnicaoService` - GPIO18 dual-debounce state machine with statistics
  - `JornadaService` - Multi-driver journey state tracking (up to 3 drivers, 7 states)
  - Audio service - minimp3 decoding + I2S playback with queueing
- Depends on: Interfaces, utilities, FreeRTOS, ESP-IDF drivers
- Used by: Main app loop via callbacks and direct queries

**Abstract Interfaces:**
- Purpose: Define service contracts, enabling implementation swapping
- Location: `include/interfaces/i_ignicao.h`, `i_jornada.h`, `i_audio.h`, `i_screen.h`
- Contains: C++ abstract base classes with both C++ and C-compatible API surfaces
- Depends on: None (zero dependencies)
- Used by: Services (implementations), application (consumers)

**UI Components:**
- Purpose: Human interface - buttons, status display, keyboards
- Location: `src/ui/widgets/`, `src/button_manager.cpp`, `src/jornada_keyboard.cpp`
- Contains:
  - `ButtonManager` - 4×3 button grid with popup dialogs, swipe navigation
  - `StatusBar` - Real-time display of ignition status, timers, messages
  - Journey/Numpad keyboards - LVGL input interfaces
  - Theme system - Centralized color, font, styling
- Depends on: LVGL, utilities, callbacks from services
- Used by: Main loop (lv_timer_handler), Touch input via LVGL

**Hardware Abstraction Layer (HAL):**
- Purpose: Encapsulate device-specific drivers
- Location: `src/hal/display/`, `src/hal/audio/`, `src/hal/gpio/`, `src/hal/touch/`, `src/hal/storage/`
- Contains:
  - LCD driver (AXS15231B QSPI)
  - Touch driver (capacitive I2C)
  - I2S audio interface
  - GPIO input (ignition pin GPIO18)
  - LittleFS filesystem bridge
- Depends on: ESP-IDF drivers, FreeRTOS
- Used by: Services, LVGL port, display BSP

**Platform / LVGL Port:**
- Purpose: LVGL-specific initialization and rendering pipeline
- Location: `src/lv_port.c`, `src/esp_bsp.c`, `src/lvgl_fs_driver.cpp`
- Contains: Display flush callback, touch callback, LVGL timer sync, LittleFS→LVGL bridge
- Depends on: LVGL, ESP-IDF, HAL drivers
- Used by: Main app loop (lv_timer_handler), LVGL core

**Utilities:**
- Purpose: Cross-cutting concerns - logging, timing, debugging
- Location: `src/utils/`, `include/utils/`
- Contains: `time_utils.h` (time_millis), `debug_utils.h` (logging macros)
- Depends on: ESP-IDF
- Used by: All layers

## Data Flow

**Ignition State Change Flow:**

1. GPIO18 reads in IgnicaoService::monitoringTask (FreeRTOS task)
2. Debounce logic applied (dual thresholds: ON=2s, OFF=1s by config)
3. State change detected → calls IgnicaoCallback (onIgnicaoStatusChange in main.cpp)
4. Callback updates:
   - UI via buttonManagerUpdateStatusBar() → StatusBar re-renders
   - Audio playback via playAudioFile() → queued to audio task (Core 1)
5. Main loop's 1s periodic update refreshes display with elapsed time

**Journey State Change Flow:**

1. UI user taps button → ButtonManager::onButtonClicked callback
2. Calls JornadaService::iniciarEstado(motoristId, newState)
3. Service updates internal motorista data structure (mutex-protected)
4. Calls JornadaCallback (onJornadaStateChange in main.cpp)
5. Main loop queries current state via JornadaService::getMotorista()
6. UI displays active state via button styling/grid updates

**Audio Playback Flow:**

1. Service or UI calls playAudioFile(filename)
2. Request queued to audio_queue (4-slot queue)
3. audio_task (Core 1) dequeues, opens file from LittleFS
4. minimp3 decodes frame-by-frame
5. PCM samples written to I2S_NUM_0 → speaker output
6. Task on Core 1 doesn't block Core 0 (UI)

**State Management:**

Service state held in protected singleton instances:
- `IgnicaoService::instance` - current ignition status + statistics
- `JornadaService::instance` - motorista array (up to 3) with state data
- Audio state - queue + playback flags

Main app only reads state via getters or receives events via callbacks; never mutates service state directly.

## Key Abstractions

**IgnicaoService (Ignition Monitor):**
- Purpose: Decouple GPIO input from business logic
- Examples: `src/services/ignicao/ignicao_service.cpp` (C++ impl), `src/ignicao_control.cpp` (legacy C wrapper)
- Pattern: Singleton with callback registration; debounce state machine; statistics tracking
- Thread safety: FreeRTOS mutex on all state access

**JornadaService (Journey State Manager):**
- Purpose: Track multiple drivers' journey states across 7 possible states
- Examples: `src/services/jornada/jornada_service.cpp`
- Pattern: Singleton; motorista ID-based lookup; time-accumulated per state
- Thread safety: Mutex-protected reads (getMotorista returns copy)

**ButtonManager (UI Grid Controller):**
- Purpose: Centralize button grid logic, avoiding scattered LVGL code
- Examples: `src/button_manager.cpp`
- Pattern: Singleton; grid occupancy tracking; dynamic button creation; popup layering
- Thread safety: Creation mutex; all LVGL calls from Core 0 only

**Theme (Styling):**
- Purpose: Prevent color/font hardcoding scattered across UI
- Examples: `src/ui/common/theme.cpp`
- Pattern: Singleton with getter methods (getColorPrimary, getFontTitle, etc.)
- Thread safety: Read-only after init

## Entry Points

**app_main() (ESP-IDF):**
- Location: `src/main.cpp`
- Triggers: Hardware startup (implicitly called by ESP-IDF framework)
- Responsibilities:
  1. Print version info via app_print_version()
  2. Mount LittleFS filesystem
  3. Initialize display hardware (AXS15231B, LVGL, backlight)
  4. Register LVGL filesystem driver (mount point 'A:')
  5. Register PNG decoder
  6. Display splash screen
  7. Create system_task on Core 0
  8. Return (task continues execution)

**system_task() (Main Loop):**
- Location: `src/main.cpp`
- Triggers: Created by app_main, pinned to Core 0
- Responsibilities:
  1. Wait for splash screen completion (busy-wait lv_timer_handler)
  2. Initialize audio subsystem (Core 1 task)
  3. Initialize UI: ButtonManager, numpad, screen manager
  4. Initialize ignition service, register onIgnicaoStatusChange callback
  5. Run 5ms LVGL event loop indefinitely:
     - Every 5ms: lv_timer_handler() processes input, animations, rendering
     - Every 1000ms: update status bar with current elapsed times

**Audio Task (Core 1):**
- Location: `src/simple_audio_manager.cpp` (audio_task function)
- Triggers: Created during audio_init()
- Responsibilities:
  1. Dequeue audio file requests from audio_queue
  2. Open MP3 file from LittleFS
  3. Decode frames using minimp3
  4. Write PCM samples to I2S_NUM_0
  5. Return when queue empty or interrupted

## Error Handling

**Strategy:** Fail-safe with logging; services continue running on non-critical errors

**Patterns:**

- **Initialization errors:** Logged but don't crash system. If audio init fails, UI still works. If ignition service init fails, app continues with fallback behavior.
  - Example: `ignicao_service.cpp` returns `false` on init failure; main.cpp logs but continues.

- **Callback errors:** Wrapped in null checks. Callbacks fire only if registered.
  - Example: Main.cpp checks `if (btnManager)` before calling buttonManagerUpdateStatusBar.

- **Hardware faults (GPIO, I2S, display):** ESP-IDF driver returns error codes; wrapped in ESP_RETURN_ON_ERROR or ESP_GOTO_ON_ERROR macros.
  - Example: `esp_bsp.c` uses bsp_err_check.h macros to halt on critical display init failure.

- **Memory allocation:** Uses heap_caps_malloc with checked returns. Audio task allocates during init; fails gracefully if no PSRAM.
  - Example: `simple_audio_manager.cpp` checks `if (!buffer)` after malloc.

- **Thread sync errors:** Mutex timeouts return false; calling code retries or skips.
  - Example: `status_bar.cpp` tries `bsp_display_lock(DISPLAY_LOCK_TIMEOUT)` with 500ms timeout.

## Cross-Cutting Concerns

**Logging:**
- Framework: ESP_LOGI, ESP_LOGW, ESP_LOGE macros from esp_log.h
- Pattern: Module-scoped static TAG (e.g., "MAIN", "IGNICAO_SVC", "BTN_MGR")
- Controlled by sdkconfig: ESP_LOG_LEVEL_* settings per module
- Example: `src/services/ignicao/ignicao_service.cpp` uses LOG_TAG("IGNICAO_SVC") + LOG_I/LOG_W/LOG_E helpers

**Validation:**
- Input validation done at service boundaries
- Button ID range checks (0 to GRID_TOTAL_BUTTONS-1)
- Motorista ID bounds checks (1-3, enforced by MAX_MOTORISTAS=3)
- File paths validated before LittleFS access
- Example: `JornadaService::addMotorista()` returns false if ID out of range

**Authentication:**
- Not applicable (embedded device, no auth required)

**Thread Safety:**
- All shared service data protected by FreeRTOS mutex (xSemaphoreCreateMutex)
- IgnicaoService, JornadaService use mutex-protected getters
- LVGL single-threaded: all LVGL calls from Core 0 only (system_task)
- Audio isolated on Core 1 with queue-based messaging
- Example: `ignicao_service.cpp` wraps getStatus with SemaphoreTake/Give

**Display Rendering Pipeline:**
1. LVGL maintains object tree in RAM (widgets, screens)
2. Every 5ms: system_task calls lv_timer_handler()
3. LVGL processes input, animations, redraws dirty regions
4. Dirty pixels flushed via lv_port.c display driver callback
5. Driver queues SPI transactions to AXS15231B LCD
6. Frame completes (TE pin sync optional)

**Timing:**
- Core system tick: 5ms (LVGL timer granularity)
- Status bar refresh: 1000ms (readable elapsed time updates)
- Ignition debounce: configurable, default ON=2s, OFF=1s
- Audio I2S write timeout: 100ms per frame
- All timekeeping via esp_timer_get_time() wrapped in time_millis() utility
