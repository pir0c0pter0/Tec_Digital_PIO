# Coding Conventions

**Analysis Date:** 2026-02-09

## Naming Patterns

**Files:**
- C/C++ source files: `snake_case.cpp` (e.g., `button_manager.cpp`, `simple_audio_manager.cpp`)
- Header files: `snake_case.h` (e.g., `app_config.h`, `ignicao_service.h`)
- Service implementations: `{service_name}_service.cpp` in `src/services/{domain}/` (e.g., `src/services/ignicao/ignicao_service.cpp`)
- Interface definitions: `i_{service_name}.h` in `include/interfaces/` (e.g., `i_ignicao.h`, `i_jornada.h`)

**Functions:**
- C functions: `snake_case` (e.g., `time_millis()`, `debug_print_heap_info()`)
- C++ class methods: `camelCase` (e.g., `getInstance()`, `getStatus()`, `setCallback()`)
- Private/internal functions: `snake_case` prefixed with underscore or marked private class section (e.g., `processDebounce()`)
- Event callbacks: descriptive names, typically verb-object (e.g., `onIgnicaoStatusChange()`, `onJornadaStateChange()`)

**Variables:**
- Global static instances: `camelCase` (e.g., `instance`, `g_audio`)
- Local variables: `camelCase` (e.g., `debounceOn`, `motoristas`)
- Configuration constants: `SCREAMING_SNAKE_CASE` (e.g., `MAX_MOTORISTAS`, `IGNICAO_PIN`, `DISPLAY_WIDTH`)
- FreeRTOS handles: descriptive names ending with type hint (e.g., `mutex`, `creationMutex`, `taskHandle`, `queueHandle`)

**Types/Classes:**
- Classes: `PascalCase` (e.g., `IgnicaoService`, `ButtonManager`, `TimeFormatter`)
- Structs: `PascalCase` or `snake_case_t` suffix (e.g., `AudioManager_t`, `DadosMotorista`)
- Enums: `PascalCase` class or `SCREAMING_SNAKE_CASE` values (e.g., `EstadoJornada::INATIVO`)
- Typedef callbacks: `PascalCase` with `Callback` suffix (e.g., `IgnicaoCallback`, `JornadaCallback`)

## Code Style

**Formatting:**
- No automated formatter configured (clang-format not in use)
- Manual adherence to patterns observed in codebase:
  - 4-space indentation (consistently applied)
  - Opening braces on same line for functions/classes
  - Opening braces on same line for control structures (`if { }`)
  - Blank lines between logical sections
  - Section separators: `// ============================================================================` (80 chars)

**Line Length:**
- Observed: typically 100-120 characters max
- Exception: long strings in logging/comments may exceed

**Linting:**
- No linter configured (no .eslintrc, no clang-tidy integration)
- Code quality relies on manual review and compilation warnings

**Compilation Warnings:**
- PlatformIO (espressif32@6.9.0) configured with no explicit warning flags
- Warnings not suppressed by default, code compiles cleanly

## Import Organization

**Order:**
1. Standard C headers (stdio.h, string.h)
2. FreeRTOS headers (freertos/FreeRTOS.h, freertos/task.h)
3. ESP-IDF headers (esp_log.h, esp_timer.h)
4. Project config headers (config/app_config.h)
5. Project interface headers (interfaces/i_*.h)
6. Project service headers (services/...)
7. Project utility headers (utils/...)
8. LVGL headers (when UI code)
9. Driver headers (pincfg.h, display.h)

**Example from `src/services/ignicao/ignicao_service.cpp`:**
```cpp
#include "services/ignicao/ignicao_service.h"
#include "config/app_config.h"
#include "utils/time_utils.h"
#include "utils/debug_utils.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
```

**Path Aliases:**
- No alias configuration (no CMake path_utils or compiler-flags aliases)
- Relative includes from project root: `#include "config/app_config.h"` not `#include "../../../config/app_config.h"`

## Error Handling

**Patterns:**
- **ESP-IDF native returns:** `esp_err_t` checked explicitly with `ESP_OK` comparison
  - Pattern: `if (ret != ESP_OK)` → log error → return or handle
  - Example in `src/core/app_init.cpp`:
    ```cpp
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            LOG_E(TAG, "Falha ao montar ou formatar filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            LOG_E(TAG, "Particao nao encontrada");
        } else {
            LOG_E(TAG, "Falha ao inicializar LittleFS: %s", esp_err_to_name(ret));
        }
        return ret;
    }
    ```

- **Pointer validation:** Always check for NULL before use
  - Pattern: `if (ptr == NULL) { return/log_error; }`
  - Example in `src/simple_audio_manager.cpp`:
    ```cpp
    static void audio_free(void** ptr) {
        if (ptr && *ptr) {
            heap_caps_free(*ptr);
            *ptr = NULL;
        }
    }
    ```

- **FreeRTOS handles:** Check handle creation success, delete on cleanup
  - Pattern: `mutex = xSemaphoreCreateMutex(); if (mutex == nullptr) { LOG_E(...); return false; }`

- **Macros for error checking:** `BSP_ERROR_CHECK_RETURN_ERR(x)`, `BSP_NULL_CHECK(x, ret)`
  - Defined in `include/bsp_err_check.h`
  - Used in legacy BSP code

- **No exceptions:** C/C++ code uses return codes, not try/catch
  - Embedded constraint: exceptions disabled in ESP-IDF

## Logging

**Framework:** ESP-IDF native logging via `esp_log.h`

**Macro layer:** Custom debug macros in `include/utils/debug_utils.h`
- `LOG_TAG(tag)`: Declares `static const char* TAG = tag` at file scope
- `LOG_I(tag, fmt, ...)`: Info level (ESP_LOGI)
- `LOG_W(tag, fmt, ...)`: Warning level (ESP_LOGW)
- `LOG_E(tag, fmt, ...)`: Error level (ESP_LOGE)
- `LOG_D(tag, fmt, ...)`: Debug level (ESP_LOGD)
- `LOG_V(tag, fmt, ...)`: Verbose level (ESP_LOGV)
- `LOG_FUNC(tag)`: Log function name and line number
- `LOG_ENTER(tag)`: Log function entry (debug level)
- `LOG_EXIT(tag)`: Log function exit (debug level)
- `LOG_ERR_CODE(tag, err)`: Log ESP error code with name

**Logging levels:**
- ERROR: Failures, exceptions, resource allocation failures
- WARNING: Recoverable issues, unexpected but handled states
- INFO: Significant events (boot, init complete, state changes)
- DEBUG: Entry/exit, internal decisions
- VERBOSE: Buffer dumps, detailed tracing

**Pattern in files:**
```cpp
// At top of .cpp file
LOG_TAG("MODULE_NAME");

// Throughout code
LOG_I(TAG, "Module initialized");
LOG_E(TAG, "Failed to init: %s", esp_err_to_name(err));
```

## Comments

**When to Comment:**
- Above complex algorithms or non-obvious logic
- Above service initialization sequences
- Explaining "why" not "what" the code does
- Interface contracts (abstract classes)

**JSDoc/Doxygen style:** Used extensively in headers
- File headers: Block comment with `=====` separators
- Function contracts in `.h`: Detailed parameter/return docs
- Implementation notes in `.cpp`: Section separators and inline comments for complex blocks

**Example from `include/interfaces/i_ignicao.h`:**
```cpp
/**
 * ============================================================================
 * INTERFACE DE IGNICAO
 * ============================================================================
 *
 * Abstrai o monitoramento da ignicao do veiculo.
 *
 * ============================================================================
 */

/**
 * Callback para mudanca de estado da ignicao
 */
typedef void (*IgnicaoCallback)(bool newStatus);

/**
 * Interface abstrata para controle de ignicao
 */
class IIgnicaoService {
public:
    /**
     * Inicializa o servico de ignicao
     * @param debounceOn Tempo de debounce para ON (segundos)
     * @param debounceOff Tempo de debounce para OFF (segundos)
     * @return true se inicializado com sucesso
     */
    virtual bool init(float debounceOn, float debounceOff) = 0;
```

**Language:** Code comments are in Portuguese (Brazilian) to match domain terminology (jornada, motorista, ignicao, manobra, descarga)

## Function Design

**Size:** Typical range 20-100 lines
- Largest production files: `button_manager.cpp` (1205 lines) due to complex LVGL widget management
- Services: `ignicao_service.cpp` (401 lines), `jornada_service.cpp` (481 lines) are well-factored
- Audio manager: `simple_audio_manager.cpp` (650 lines) with clear helper functions

**Parameters:**
- Max 4-5 parameters typical
- Callbacks passed as function pointers: `IgnicaoCallback callback`
- Output parameters: pointer to struct (e.g., `DadosMotorista* dados`)
- No out-of-order or optional parameters (embedded constraint)

**Return Values:**
- `bool` for success/failure
- `esp_err_t` for ESP-IDF operations
- Pointers for object references
- Enums for state machines
- `void` for callbacks and notifications

## Module Design

**Exports:**
- Interface first: public API declared in `include/interfaces/i_*.h` (abstract classes)
- Implementation: concrete class in `src/services/{domain}/{name}_service.cpp`
- Singleton pattern: `static getInstance()` and `static destroyInstance()` methods
- Example: `IgnicaoService::getInstance()` → returns singleton, never created multiple times

**Barrel Files (Index Headers):**
- Not extensively used
- Each module has a single public header in `include/` or `include/interfaces/`
- No umbrella headers that re-export multiple interfaces

**Internal Headers:**
- Service implementation headers in `src/services/{domain}/` (not re-exported)
- Example: `include/services/ignicao/ignicao_service.h` is public (concrete implementation)
- Utility headers in `include/utils/` (time_utils.h, debug_utils.h)

## Thread Safety

**Synchronization:**
- All services use FreeRTOS `SemaphoreHandle_t` mutex for shared state
- Pattern: Acquire mutex → copy/modify state → release mutex
- Example from `ignicao_service.cpp`:
  ```cpp
  bool IgnicaoService::getStatus() const {
      if (!mutex) return status;
      xSemaphoreTake(mutex, portMAX_DELAY);
      bool result = status;
      xSemaphoreGive(mutex);
      return result;
  }
  ```

**Callbacks:**
- Async notification pattern: Event producers register callbacks
- Callbacks execute from producer's context (ISR or task)
- Example: `IgnicaoCallback` invoked when ignition state changes
- Callbacks must be non-blocking (no mutex acquisition)

**Core Affinity:**
- Core 0: UI rendering (LVGL), main event loop (system_task)
- Core 1: Audio processing (minimp3 decode + I2S playback)
- Enforced by `AUDIO_TASK_CORE = 1` in config

## C/C++ Mixed Code

**C modules:**
- Legacy drivers: `esp_lcd_axs15231b.c`, `esp_lcd_touch.c`, `lv_port.c`
- Wrapped with `extern "C" { }` when called from C++
- Use `#ifdef __cplusplus` guards in headers

**C++ modules:**
- Services: `ignicao_service.cpp`, `jornada_service.cpp`, `button_manager.cpp`
- UI components: `status_bar.cpp`, `theme.cpp`
- Utilities: `time_utils.cpp`, `simple_audio_manager.cpp`

**Compatibility layer:**
- Mixed code compiled with same compiler (espressif32 C++ compiler)
- Function linkage via `extern "C"` declarations in main.cpp

---

*Convention analysis: 2026-02-09*
