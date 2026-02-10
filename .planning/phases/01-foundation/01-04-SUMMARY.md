---
phase: 01-foundation
plan: 04
subsystem: nvs-persistence
tags: [nvs, persistence, settings, jornada-state, nvs_data, esp-idf]

# Dependency graph
requires:
  - "Partition table with nvs_data partition (from 01-01)"
  - "NVS constants in app_config.h: NVS_PARTITION_LABEL, NVS_NS_SETTINGS, NVS_NS_JORNADA, NVS_KEY_VOLUME, NVS_KEY_BRIGHTNESS, NVS_JORNADA_VERSION (from 01-01)"
  - "JornadaScreen with JornadaKeyboard per-screen instance (from 01-03, 01.1)"
provides:
  - "INvsManager abstract interface for NVS persistence"
  - "NvsManager singleton using dedicated nvs_data partition with mutex and error recovery"
  - "Volume and brightness restored at boot from NVS"
  - "Journey keyboard state (motorist action bitmap) auto-saved on state change and restored on screen create"
  - "JornadaKeyboard::StateChangeCallback for decoupled state change notification"
affects: [02-ble-core, 03-settings-screen, 04-ota]

# Tech tracking
tech-stack:
  added: [nvs_flash, nvs]
  patterns:
    - "INvsManager interface-first pattern matching existing i_jornada.h, i_audio.h, i_screen.h"
    - "NvsManager singleton with FreeRTOS mutex for thread safety"
    - "nvs_open_from_partition() for dedicated partition access (not default NVS)"
    - "Packed struct with version field for future migration"
    - "Compact bitmap (uint16_t) for per-motorist action state persistence"
    - "StateChangeCallback on JornadaKeyboard for decoupled NVS auto-save"

key-files:
  created:
    - "include/interfaces/i_nvs.h"
    - "include/services/nvs/nvs_manager.h"
    - "src/services/nvs/nvs_manager.cpp"
  modified:
    - "src/main.cpp"
    - "include/jornada_keyboard.h"
    - "src/jornada_keyboard.cpp"
    - "include/ui/screens/jornada_screen.h"
    - "src/ui/screens/jornada_screen.cpp"

key-decisions:
  - "Use NvsKbMotoristaState (3 bytes bitmap) instead of NvsJornadaState for keyboard state -- matches JornadaKeyboard per-action-per-motorist model"
  - "Auto-save via StateChangeCallback on processarAcao -- saves on every state change for power-loss resilience"
  - "Additional save on onExit and destroy as safety net"
  - "Restore on create() (not onEnter) -- state loaded once when screen is built at boot"
  - "tempoInicio reset to millis() on restore -- acceptable loss of in-flight time per plan"

patterns-established:
  - "New services: interface in include/interfaces/, header in include/services/<name>/, impl in src/services/<name>/"
  - "NVS access always via NvsManager singleton -- never direct nvs_* calls elsewhere"
  - "State change callbacks for decoupled persistence (not direct NVS calls from UI)"

# Metrics
duration: 5min
completed: 2026-02-10
---

# Phase 1 Plan 04: NVS Persistence Service Summary

**NvsManager service with settings (volume/brightness) and journey state (per-motorist action bitmap) persistence using dedicated nvs_data partition, integrated with boot sequence and JornadaScreen auto-save**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-10
- **Completed:** 2026-02-10
- **Tasks:** 2 of 3 (Task 3 is checkpoint:human-verify, pending hardware verification)
- **Files created:** 3
- **Files modified:** 5
- **Commit:** `1c46e6f`

## Accomplishments

- Created INvsManager abstract interface with C compatibility layer
- Created NvsManager implementation using nvs_data partition with mutex, error recovery (erase on corruption), version validation
- NvsJornadaState packed struct with version field for future migration
- Volume restored at boot via setAudioVolume(), brightness via bsp_display_brightness_set()
- JornadaKeyboard StateChangeCallback fires on every processarAcao()
- JornadaScreen saves compact bitmap (3 bytes/motorist) on state change, on exit, and on destroy
- JornadaScreen restores motorist action states from NVS on create()

## Files Created/Modified

- `include/interfaces/i_nvs.h` - INvsManager interface + C wrappers
- `include/services/nvs/nvs_manager.h` - NvsManager class + NvsJornadaState packed struct
- `src/services/nvs/nvs_manager.cpp` - Full NVS implementation with partition init, settings, journey blobs
- `src/main.cpp` - NVS init before audio, volume/brightness restore at boot
- `include/jornada_keyboard.h` - StateChangeCallback typedef + member
- `src/jornada_keyboard.cpp` - Callback init, setter, call in processarAcao
- `include/ui/screens/jornada_screen.h` - saveStateToNvs/restoreStateFromNvs declarations
- `src/ui/screens/jornada_screen.cpp` - NVS save/restore with NvsKbMotoristaState bitmap

## Decisions Made

- NvsKbMotoristaState bitmap instead of NvsJornadaState for keyboard state -- JornadaKeyboard uses per-action-per-motorist model (not single estado per motorist)
- StateChangeCallback on JornadaKeyboard for auto-save -- decouples persistence from domain logic
- Restore on create() not onEnter() -- state loaded once when screen is built at boot

## Deviations from Plan

- Added jornada_keyboard.h/cpp modifications (not in plan's files_modified) to implement StateChangeCallback
- Used NvsKbMotoristaState (bitmap) instead of NvsJornadaState (DadosMotorista-style) -- plan's struct doesn't match JornadaKeyboard's actual state model

## Next Phase Readiness

- Task 3 (hardware verification) pending -- power cycle test to confirm NVS persistence
- Phase 1 Foundation complete after hardware verification
- Phase 2 (BLE Core) can proceed -- NVS ready for bond persistence

---
*Phase: 01-foundation*
*Completed: 2026-02-10 (pending hardware verification)*
