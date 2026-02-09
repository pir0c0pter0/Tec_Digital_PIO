# Testing Patterns

**Analysis Date:** 2026-02-09

## Test Framework

**Status:** No automated test framework configured

**Why:** Bare-metal embedded firmware verified by hardware flashing
- Platform: ESP32-S3 with FreeRTOS
- Testing method: Flash to hardware and observe behavior
- Alternative: Manual testing via serial monitor (115200 baud)
- No CI/CD test pipeline configured

**Build Verification:**
```bash
# Build (syntax/link verification)
pio run

# Flash to hardware
pio run --target upload

# Filesystem upload
pio run --target uploadfs

# Serial monitor (manual observation)
pio device monitor
```

## Test File Organization

**Location:** No test files exist in codebase
- No `test/`, `tests/`, `__tests__` directories
- No `*.test.cpp`, `*.spec.cpp`, `*.test.h` files
- Managed components (joltwallet__littlefs, LVGL) contain internal tests not used by this project

**Integration Testing:**
- Hardware integration tests performed manually
- Procedure: Flash firmware → observe serial output → verify behavior
- Critical flows tested:
  1. Ignition state change (GPIO18 monitoring)
  2. Journey state transitions (button presses)
  3. Audio playback (MP3 from LittleFS)
  4. Display rendering (LVGL UI updates)

## Verification Approach

**Serial Monitor Output:**
- Tag-based logging allows tracing of operations
- Example: Monitor "IGNICAO_SVC" tag for ignition state changes
- All significant events logged with timestamps via `esp_timer_get_time()`

**Logging Categories:**
- `[MAIN]`: Core system state, ignition changes
- `[IGNICAO_SVC]`: Ignition service events (init, state changes, debounce)
- `[JORNADA_SVC]`: Journey state machine events (motorista added, estado changed)
- `[AUDIO]`: Audio playback (file open, decode start/stop)
- `[BTN_MGR]`: Button manager (grid creation, clicks)
- `[APP_INIT]`: Initialization sequence (filesystem, services)

**Manual Test Checklist (from CLAUDE.md):**
1. Build firmware: `pio run` (no compile errors)
2. Flash firmware: `pio run --target upload` (success message)
3. Flash filesystem: `pio run --target uploadfs` (LittleFS mounted)
4. Monitor startup sequence on serial console
5. Test ignition GPIO18: connect/disconnect, verify debounce
6. Test button grid: tap buttons, verify journey state changes
7. Test audio: trigger audio events, verify MP3 playback
8. Test display: verify LVGL rendering, no visual artifacts

## Code Analysis for Testability

**Unit Test Candidates (if framework were added):**
- `time_utils.cpp`: Time formatting and conversion functions (pure functions, no side effects)
  - `time_millis()`, `time_format_ms()`, `time_elapsed_since()`
  - Isolated logic, easily mockable
  - Path: `src/utils/time_utils.cpp`

- `simple_audio_manager.cpp`: Audio queue management
  - `playAudioFile()`, `stopAudio()`, `setVolume()`
  - Encapsulated audio state machine
  - Path: `src/simple_audio_manager.cpp`

- `ignicao_service.cpp`: Debounce logic
  - `processDebounce()` (private method)
  - State machine for debounce timing
  - Path: `src/services/ignicao/ignicao_service.cpp`

- `jornada_service.cpp`: State machine for journey tracking
  - Estado transitions, motorista management
  - Callback notifications
  - Path: `src/services/jornada/jornada_service.cpp`

**Integration Test Candidates:**
- Ignition service + callback: GPIO state → debounce → callback invocation
- Journey service + ignition: Ignition OFF → reset journey states
- Audio playback + filesystem: LittleFS mount → load MP3 → I2S output
- Button click + UI update: Button press → LVGL event → status bar update

**Hard to Test (Embedded Constraints):**
- LVGL UI rendering: Requires display hardware or framebuffer mock
- I2S audio output: Requires audio hardware
- GPIO hardware: Requires ESP32-S3 with proper pin configuration
- FreeRTOS task scheduling: Race conditions, task priority interactions

## Mocking Strategy (If Test Framework Added)

**Potential Mock Targets:**
- `esp_timer_get_time()`: Return fixed time values for time-based tests
- `gpio_get_level()`: Return mock GPIO pin states for ignition tests
- `xSemaphoreCreateMutex()`: Return mock semaphore handles
- `fopen()`: Return mock FILE* for filesystem tests
- `lv_obj_create()`: Return mock LVGL object handles

**What NOT to Mock:**
- FreeRTOS core (xTaskCreate, vTaskDelay): Too low-level, fragile mocks
- I2S driver: Hardware interface, would require full driver mock
- Display driver: Complex register operations, better to test on hardware

## Code Coverage

**Current Status:** Not measured
- No gcov configuration
- No coverage reporting tool
- No coverage targets set

**Estimated Coverage (by observation):**
- Core service logic: 70-80% (main paths exercised, error paths untested)
- Utility functions: 90%+ (time_utils, debug_utils heavily used)
- UI event handlers: 50% (depends on button press patterns)
- Error paths: <30% (no systematic error injection testing)

**Gaps:**
- GPIO error handling (GPIO configuration failures)
- Memory allocation failures (malloc/heap_caps_malloc)
- Filesystem I/O errors (file open/read failures)
- FreeRTOS resource exhaustion (task creation fails, semaphore fails)
- Audio edge cases (corrupted MP3, unsupported format)

## Testing Approach If Framework Were Added

**Recommended Framework:** GoogleTest (gtest) with mocking (gmock)
- Cross-platform (compile on Linux/Mac, run on device or emulator)
- Supports FreeRTOS code with careful isolation
- Header-only library, minimal dependencies

**Test Structure (Hypothetical):**
```cpp
// test/ignicao_service_test.cpp
#include <gtest/gtest.h>
#include "services/ignicao/ignicao_service.h"
#include "utils/time_utils.h"

class IgnicaoServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        service = IgnicaoService::getInstance();
        service->init(1.0f, 2.0f);
    }

    void TearDown() override {
        IgnicaoService::destroyInstance();
    }

    IgnicaoService* service;
};

TEST_F(IgnicaoServiceTest, DebounceOnWorks) {
    // Mock GPIO to return HIGH
    // Simulate time passage to trigger debounce timer
    // Verify callback invoked after debounceOn seconds
}

TEST_F(IgnicaoServiceTest, DebounceOffWorks) {
    // Similar test for OFF debounce
}
```

**Run Command:**
```bash
# Build tests with espressif32 toolchain
pio run -e test

# Or compile for Linux (with mocked HAL)
g++ -I include -I managed_components -o test_ignicao test/ignicao_service_test.cpp src/services/ignicao/ignicao_service.cpp -lgtest -lgtest_main
./test_ignicao
```

## Performance Testing

**No Benchmarking Tool Configured**

**Performance Characteristics (Observed):**
- Ignition debounce: Configurable 1-2 seconds
- Button response: 300ms debounce (BUTTON_DEBOUNCE_MS in button_manager.cpp)
- Audio decode: Runs on Core 1, doesn't block UI (Core 0)
- LVGL refresh: 5ms system tick in main.cpp

**Potential Bottlenecks:**
- Button manager: 1205 lines, complex LVGL widget management
- Audio decoding: minimp3 single-threaded, limited by MP3 bitrate
- Status bar updates: 250ms interval (STATUS_BAR_UPDATE_MS)

## Hardware Testing

**Test Hardware:**
- ESP32-S3 DevKit with 16MB flash, PSRAM
- 3.5" AXS15231B LCD (480×320 native, rotated 90° → 320×480 working)
- Capacitive I2C touch input
- I2S audio output to speaker

**Flash Partitions:**
```
app0:    3MB (factory app)
spiffs:  1MB (LittleFS - audio MP3s + splash PNG)
```

**Verification Commands:**
```bash
# Serial monitor with logging
pio device monitor

# Monitor specific tag
pio device monitor --filter "IGNICAO_SVC"

# Full clean build + flash
pio run --target clean && pio run && pio run --target upload

# Filesystem debug
pio run --target uploadfs && pio device monitor
```

## Error Injection Testing (Manual)

**Tested Scenarios:**
- Ignition GPIO transient: Toggle GPIO18 rapidly, verify debounce holds
- Button spam: Tap multiple buttons in sequence, verify queue handling
- Audio underrun: Decode delay, verify no audio glitches
- Display state: UI components added/removed, verify LVGL consistency

**Untested Scenarios:**
- Power loss during audio playback
- Filesystem corruption during read
- FreeRTOS task starvation
- PSRAM access failures
- I2C touch sensor timeout

## Continuous Integration

**No CI Pipeline Configured**
- No GitHub Actions workflow
- No automated build on push
- Release process (if any) requires manual compilation and hex file upload

**Build-Related Files:**
- `platformio.ini`: Build configuration (esp32s3_idf environment)
- `.pio/`: Build artifacts (auto-generated)
- `sdkconfig`: ESP-IDF configuration

---

*Testing analysis: 2026-02-09*
