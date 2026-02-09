# Codebase Structure

**Analysis Date:** 2026-02-09

## Directory Layout

```
sketch_oct8a_pio/
├── src/                          # Source implementation files
│   ├── main.cpp                  # App entry point, system_task event loop
│   ├── core/                     # System initialization
│   │   └── app_init.cpp          # Filesystem, version, startup coordination
│   ├── services/                 # Business logic services
│   │   ├── ignicao/
│   │   │   └── ignicao_service.cpp
│   │   ├── jornada/
│   │   │   └── jornada_service.cpp
│   │   └── audio/                # (audio service implementations)
│   ├── ui/                       # User interface components
│   │   ├── widgets/
│   │   │   └── status_bar.cpp    # Ignition/timer/message display
│   │   ├── screens/              # (screen implementations - empty)
│   │   └── common/
│   │       └── theme.cpp         # Centralized styling
│   ├── hal/                      # Hardware abstraction layer (drivers)
│   │   ├── display/              # LCD/QSPI drivers
│   │   ├── audio/                # I2S audio interface
│   │   ├── gpio/                 # GPIO input/output
│   │   ├── touch/                # Touch input (I2C)
│   │   └── storage/              # LittleFS interface
│   ├── utils/                    # Utilities
│   │   ├── time_utils.cpp        # Millisecond timer wrapper
│   │   └── debug_utils.cpp       # Logging helpers
│   ├── esp_bsp.c                 # Board support package (display init, backlight)
│   ├── esp_lcd_axs15231b.c       # AXS15231B LCD QSPI controller
│   ├── esp_lcd_touch.c           # Capacitive touch I2C driver
│   ├── lv_port.c                 # LVGL port (display flush, touch input)
│   ├── lvgl_fs_driver.cpp        # LittleFS ↔ LVGL bridge
│   ├── simple_audio_manager.cpp  # Audio playback + minimp3 decoder (Core 1)
│   ├── simple_splash.cpp         # PNG splash screen loader
│   ├── button_manager.cpp        # 4×3 button grid UI controller
│   ├── jornada_keyboard.cpp      # Journey state selection keyboard
│   ├── numpad_example.cpp        # Number input keyboard
│   ├── jornada_manager.cpp       # (legacy wrapper, see JornadaService)
│   ├── ignicao_control.cpp       # (legacy wrapper, see IgnicaoService)
│   └── simple_ui.cpp             # (legacy simple UI, mostly unused)
├── include/                      # Header files (parallel structure to src/)
│   ├── config/
│   │   └── app_config.h          # All 78+ system constants (pins, colors, timings)
│   ├── core/
│   │   └── app_init.h            # app_init_filesystem(), app_print_version()
│   ├── interfaces/               # Abstract service interfaces
│   │   ├── i_ignicao.h           # IIgnicaoService abstract base class
│   │   ├── i_jornada.h           # IJornadaService abstract base class
│   │   ├── i_audio.h             # IAudioPlayer abstract base class
│   │   └── i_screen.h            # (screen interface)
│   ├── services/                 # Service headers (mirror src/ structure)
│   │   ├── ignicao/
│   │   │   └── ignicao_service.h
│   │   └── jornada/
│   │       └── jornada_service.h
│   ├── ui/
│   │   ├── widgets/
│   │   │   └── status_bar.h
│   │   └── common/
│   │       └── theme.h
│   ├── utils/
│   │   ├── time_utils.h          # time_millis() function
│   │   └── debug_utils.h         # LOG_TAG, LOG_I, LOG_W, LOG_E macros
│   ├── pincfg.h                  # GPIO/SPI/I2C pin assignments
│   ├── bsp_err_check.h           # Error handling macros
│   ├── simple_audio_manager.h    # Audio API
│   ├── simple_splash.h
│   ├── button_manager.h
│   ├── jornada_keyboard.h
│   ├── numpad_example.h
│   ├── ignicao_control.h         # (legacy C interface)
│   ├── jornada_manager.h
│   ├── esp_bsp.h
│   ├── esp_lcd_axs15231b.h
│   ├── esp_lcd_touch.h
│   ├── lvgl_fs_driver.h
│   ├── lv_port.h
│   ├── display.h
│   ├── lv_conf.h                 # LVGL configuration (port settings)
│   └── minimp3.h                 # MP3 decoder header-only library
├── lib/                          # External libraries (checked in)
│   ├── lvgl/                     # LVGL 8.4.0 (complete source)
│   │   ├── src/                  # Core LVGL library
│   │   ├── examples/             # LVGL example code
│   │   └── demos/                # LVGL demo applications
│   └── ESP32-audioI2S/           # Audio I2S library (reference only)
├── managed_components/           # PlatformIO dependencies
│   └── joltwallet__littlefs/     # LittleFS component
├── components/                   # (empty, for future ESP-IDF components)
├── data/                         # Filesystem contents (LittleFS)
│   ├── audio MP3 files           # 11 audio files (paths in app_config.h)
│   └── splash.png                # Splash screen image
├── docs/                         # Documentation
│   └── (architecture notes, pinout diagrams)
├── platformio.ini                # Build configuration
├── CMakeLists.txt                # (optional ESP-IDF build)
├── CLAUDE.md                     # Project guidelines for Claude
└── README.md                     # Project overview
```

## Directory Purposes

**src/:**
- Purpose: All C++ implementation files
- Contains: Service implementations, UI controllers, HAL drivers, utilities, main loop
- Key structure: Organized by responsibility (services, ui, hal, utils, core)

**include/:**
- Purpose: Public headers (same structure as src)
- Contains: Interfaces, constants, declarations, public APIs
- Key files:
  - `config/app_config.h` - Single source of truth for all constants (pins, timings, colors, limits)
  - `interfaces/` - Abstract classes defining service contracts
  - `pincfg.h` - Hardware pin assignments (GPIO, SPI, I2C)
  - `lv_conf.h` - LVGL library configuration

**lib/:**
- Purpose: Vendored dependencies
- Contains: LVGL 8.4.0 source (full library), example code, demos
- Not built directly; included via CMakeLists/platformio.ini

**managed_components/:**
- Purpose: PlatformIO-downloaded dependencies
- Contains: LittleFS component (installed via platform package manager)
- Updated automatically by `pio pkg install`

**data/:**
- Purpose: Filesystem contents (mounted as LittleFS at runtime)
- Contains: 11 MP3 audio files + splash.png
- Uploaded to 1MB SPIFFS partition via `pio run --target uploadfs`
- File paths referenced in `app_config.h` (e.g., AUDIO_FILE_IGN_ON = "A:/ignicao_on.mp3")

**docs/:**
- Purpose: Project documentation
- Contains: Architecture notes, pinout diagrams, build instructions
- Not compiled; reference only

## Key File Locations

**Entry Points:**
- `src/main.cpp:app_main()` - Called by ESP-IDF framework at startup
- `src/main.cpp:system_task()` - Main event loop (Core 0, LVGL ticker, status updates)
- `src/simple_audio_manager.cpp:audio_task()` - Audio decode/playback (Core 1)

**Configuration:**
- `include/config/app_config.h` - All constants (MUST edit here, never hardcode elsewhere)
- `include/pincfg.h` - Hardware pin mappings
- `platformio.ini` - Build flags, board, dependencies
- `include/lv_conf.h` - LVGL feature toggles

**Core Logic:**
- `src/services/ignicao/ignicao_service.cpp` - Ignition GPIO monitoring with debounce
- `src/services/jornada/jornada_service.cpp` - Journey state machine for up to 3 drivers
- `src/simple_audio_manager.cpp` - Audio queue + minimp3 decoding
- `src/button_manager.cpp` - Button grid controller with popup dialogs

**UI Components:**
- `src/button_manager.cpp` - 4×3 button grid layout and event handling
- `src/ui/widgets/status_bar.cpp` - Status display (ignition, timers, messages)
- `src/ui/common/theme.cpp` - Centralized color/font definitions
- `src/jornada_keyboard.cpp` - Journey state selection UI
- `src/numpad_example.cpp` - Number input UI

**Hardware Interface:**
- `src/esp_bsp.c` - Display initialization, backlight control
- `src/esp_lcd_axs15231b.c` - AXS15231B LCD controller (QSPI protocol)
- `src/esp_lcd_touch.c` - Capacitive touch input (I2C)
- `src/lv_port.c` - LVGL display flush callback, touch polling
- `src/lvgl_fs_driver.cpp` - LittleFS ↔ LVGL bridge (mounts as 'A:')

**Utilities:**
- `src/utils/time_utils.cpp` - time_millis() implementation
- `src/utils/debug_utils.cpp` - LOG_TAG and logging macros
- `src/core/app_init.cpp` - Filesystem mount, version printing

**Testing:**
- No test framework present (bare-metal embedded, verified by hardware flashing)

## Naming Conventions

**Files:**
- Source: `snake_case.cpp` (e.g., `button_manager.cpp`, `ignicao_service.cpp`)
- Headers: `snake_case.h` (e.g., `app_config.h`, `status_bar.h`)
- Interfaces: `i_snake_case.h` prefix for abstract interfaces (e.g., `i_ignicao.h`)
- Legacy C code: `.c` extension (e.g., `esp_bsp.c`, `lv_port.c`)

**Classes:**
- PascalCase for C++ classes (e.g., `ButtonManager`, `IgnicaoService`, `StatusBar`, `Theme`)
- Suffix pattern: `*Service` for domain services, `*Manager` for controllers, `*Bar` for UI widgets

**Functions:**
- C++ methods: camelCase (e.g., `getStatus()`, `iniciarEstado()`, `onButtonClicked()`)
- C functions: snake_case (e.g., `time_millis()`, `app_init_filesystem()`)
- Callbacks: camelCase or `on*` prefix (e.g., `onIgnicaoStatusChange()`, `onJornadaStateChange()`)

**Variables:**
- Local: camelCase (e.g., `ignicaoLigada`, `motoristId`, `tempoIgnicao`)
- Static/global: camelCase with type-indicating prefix if needed (e.g., `statusBar`, `motoristas`)
- Constants: UPPER_SNAKE_CASE (e.g., `GRID_COLS`, `STATUS_BAR_HEIGHT`, `IGNICAO_DEBOUNCE_ON_S`)
- Portuguese domain terms kept: `jornada`, `motorista`, `ignicao`, `manobra`

**Directories:**
- Feature domains: singular or plural, lowercase (e.g., `services/`, `ui/`, `hal/`)
- Subdomains: organizing principle (e.g., `ignicao/`, `jornada/` for services; `widgets/`, `screens/` for UI)

## Where to Add New Code

**New Feature (e.g., battery monitoring service):**
1. Create interface: `include/interfaces/i_battery.h` (abstract class)
2. Create implementation:
   - `src/services/battery/battery_service.cpp` (main logic)
   - `include/services/battery/battery_service.h` (public API)
3. Add config constants: Edit `include/config/app_config.h` (pin, thresholds, etc.)
4. Integrate in main loop: Edit `src/main.cpp` (init in system_task, call in event loop)
5. Add tests if framework existed; for embedded, verify on hardware

**New UI Component (e.g., battery indicator widget):**
1. Create widget:
   - `src/ui/widgets/battery_indicator.cpp` (implementation)
   - `include/ui/widgets/battery_indicator.h` (header)
2. Styling: Add colors/fonts to `src/ui/common/theme.cpp` if needed
3. Integration: Instantiate in parent screen (usually done in `button_manager.cpp` or status bar)
4. Testing: Flash firmware and verify on device

**New Utility Function (e.g., CRC checksum):**
- Single file per domain: `src/utils/crc_utils.cpp` + `include/utils/crc_utils.h`
- Keep to <400 lines per utility file; if larger, split into separate domains
- Always include module-level LOG_TAG for debugging

**Modify Existing Service (e.g., add new journey state):**
1. Update interface in `include/interfaces/i_jornada.h` (add to EstadoJornada enum)
2. Update implementation: `src/services/jornada/jornada_service.cpp` (handle new state)
3. Update constants: `include/config/app_config.h` (NUM_ESTADOS_JORNADA if needed)
4. Update UI buttons: `src/button_manager.cpp` (add button for new state)
5. Update main loop: Add callback handling if needed in `src/main.cpp`

## Special Directories

**include/config/:**
- Purpose: Centralized system constants
- Generated: No (manually maintained)
- Committed: Yes (essential for build)
- Contents: All pins, colors, sizes, timings, limits, file paths
- Rationale: Single source of truth prevents config duplication

**data/:**
- Purpose: LittleFS filesystem contents
- Generated: No (pre-built assets)
- Committed: Yes (audio MP3s, splash PNG)
- Upload: Via `pio run --target uploadfs` (separate from firmware binary)
- Access: At runtime via LVGL file driver (mount point 'A:'), e.g., "A:/ignicao_on.mp3"

**lib/lvgl/:**
- Purpose: Vendored LVGL library (v8.4.0)
- Generated: No (external library, checked in)
- Committed: Yes (entire library for reproducible builds)
- Configuration: Via `include/lv_conf.h` (feature toggles, buffer sizes)
- Note: Very large directory (~100MB+ with demos/examples); can be pruned for distribution

**managed_components/:**
- Purpose: PlatformIO package manager downloads
- Generated: Yes (created by `pio pkg install`)
- Committed: No (gitignored, regenerated from lock file)
- Contents: LittleFS ESP32 component
- Update: Run `pio update` to refresh

**include/interfaces/:**
- Purpose: Abstract service contracts (zero implementation)
- Generated: No (hand-written)
- Committed: Yes (critical for architecture)
- Pattern: All interfaces use C++ abstract base classes with extern "C" fallbacks for C compatibility
- Principle: Depend on abstractions, not implementations

**build/ & .pio/:**
- Purpose: Build artifacts
- Generated: Yes (by PlatformIO/CMake)
- Committed: No (gitignored)
- Contents: Object files, dependency caches, firmware binaries
- Clean: `pio run --target clean` or `rm -rf .pio build`
