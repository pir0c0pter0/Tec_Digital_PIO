---
phase: 01-foundation
plan: 03
subsystem: ui-screens
tags: [jornada-screen, numpad-screen, screen-manager, lvgl, iscreen, navigation, button-manager]

# Dependency graph
requires:
  - "ScreenManagerImpl with push/pop stack navigation and animated transitions (from 01-02)"
  - "StatusBar on lv_layer_top() persistent across screen transitions (from 01-02)"
  - "Centralized Screen Navigation constants in app_config.h (from 01-01)"
provides:
  - "JornadaScreen implementing IScreen with 12 journey action buttons via JornadaKeyboard"
  - "NumpadScreen implementing IScreen with digit input via NumpadExample"
  - "main.cpp boot sequence using ScreenManagerImpl instead of old ButtonManager/ScreenManager"
  - "StatusBar integrated with ScreenManager for menu/back navigation in main loop"
  - "Ignition callback using StatusBar.setIgnicao() directly"
affects: [01-04, 03-settings-screen, 04-ota, 05-rpm]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Screen classes wrap existing singleton domain logic (JornadaKeyboard, NumpadExample)"
    - "ButtonManager used as shared rendering engine, screen_ obtained from lv_scr_act() after init"
    - "Static allocation of screen objects and StatusBar in main.cpp (no heap)"
    - "Ignition data flows: main loop -> StatusBar.update(StatusBarData), callback -> StatusBar.setIgnicao()"

key-files:
  created:
    - "include/ui/screens/jornada_screen.h"
    - "src/ui/screens/jornada_screen.cpp"
    - "include/ui/screens/numpad_screen.h"
    - "src/ui/screens/numpad_screen.cpp"
  modified:
    - "src/main.cpp"

key-decisions:
  - "Wrap existing JornadaKeyboard/NumpadExample rather than rewrite -- preserves all v1.x behavior"
  - "Screen classes delegate LVGL rendering to ButtonManager singleton -- minimizes code duplication"
  - "Static allocation of screens and StatusBar in main.cpp -- no heap for predictable embedded behavior"
  - "Ignition callback uses StatusBar.setIgnicao() directly instead of old buttonManagerUpdateStatusBar()"

patterns-established:
  - "New IScreen implementations should wrap domain logic classes, not duplicate them"
  - "Screen create() initializes ButtonManager then creates domain-specific content"
  - "main.cpp creates StatusBar, ScreenManager, registers screens, then shows initial screen"

# Metrics
duration: 3min
completed: 2026-02-09
---

# Phase 1 Plan 03: Screen Integration Summary

**JornadaScreen and NumpadScreen as IScreen wrappers around existing domain logic, integrated with ScreenManagerImpl in main.cpp boot sequence for animated navigation via StatusBar menu/back buttons**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-09T21:27:45Z
- **Completed:** 2026-02-09T21:31:17Z
- **Tasks:** 2 of 3 (Task 3 is checkpoint:human-verify, pending hardware verification)
- **Files created:** 4
- **Files modified:** 1

## Accomplishments

- Created JornadaScreen implementing IScreen that wraps JornadaKeyboard for 12 journey action buttons with motorist state management, popup selection, and audio feedback
- Created NumpadScreen implementing IScreen that wraps NumpadExample for digit input with timeout, validation, and audio feedback
- Rewrote main.cpp boot sequence: old ButtonManager/ScreenManager direct init replaced by ScreenManagerImpl with registered screens, persistent StatusBar, and animated navigation
- Ignition data flow modernized: StatusBar.update() and StatusBar.setIgnicao() replace buttonManagerUpdateStatusBar()

## Task Commits

Each task was committed atomically:

1. **Task 1: Create JornadaScreen and NumpadScreen as IScreen implementations** - `bb803a5` (feat)
2. **Task 2: Integrate screens with ScreenManagerImpl and update main.cpp boot sequence** - `f759eb8` (feat)
3. **Task 3: Verify screen navigation on hardware** - PENDING (checkpoint:human-verify)

## Files Created/Modified

- `include/ui/screens/jornada_screen.h` - JornadaScreen class declaration implementing IScreen
- `src/ui/screens/jornada_screen.cpp` - JornadaScreen wrapping JornadaKeyboard for journey button grid
- `include/ui/screens/numpad_screen.h` - NumpadScreen class declaration implementing IScreen
- `src/ui/screens/numpad_screen.cpp` - NumpadScreen wrapping NumpadExample for digit input
- `src/main.cpp` - Rewritten boot sequence using ScreenManagerImpl, StatusBar, registered screens

## Decisions Made

- Wrap existing JornadaKeyboard/NumpadExample singletons rather than rewriting their logic -- preserves all v1.x behavior and avoids introducing bugs
- Screen classes obtain their lv_obj_t* from lv_scr_act() after ButtonManager.init() creates the screen -- leverages existing rendering pipeline
- Static allocation for screens and StatusBar in main.cpp -- avoids heap allocation, predictable embedded memory
- Ignition callback uses StatusBar.setIgnicao() directly -- cleaner than routing through old buttonManagerUpdateStatusBar() C wrapper

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Task 3 (hardware verification) is pending -- requires flashing and testing on actual device
- Once verified, the v2 screen navigation framework is fully operational
- Future screens (Settings, OTA, RPM) can follow the same pattern: create IScreen wrapper, register with ScreenManagerImpl
- All v1.x functionality preserved (journey buttons, numpad input, audio feedback, ignition monitoring)

## Self-Check: PENDING

Self-check will be completed after Task 3 hardware verification.

---
*Phase: 01-foundation*
*Completed: 2026-02-09 (pending hardware verification)*
