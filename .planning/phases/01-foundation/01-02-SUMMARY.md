---
phase: 01-foundation
plan: 02
subsystem: ui-navigation
tags: [screen-manager, status-bar, navigation, lvgl, animation, lv_layer_top]

# Dependency graph
requires:
  - "Centralized Screen Navigation constants in app_config.h (from 01-01)"
provides:
  - "ScreenManagerImpl with push/pop stack navigation and animated transitions"
  - "StatusBar on lv_layer_top() persistent across screen transitions"
  - "Menu button for screen cycling (JORNADA <-> NUMPAD)"
  - "Back button with visibility controlled by navigation stack depth"
  - "IScreen.getLvScreen() method for LVGL screen access"
  - "OTA and RPM entries in ScreenType enum (for future phases)"
  - "C wrapper functions for screen navigation (screen_manager_init, etc.)"
affects: [01-03, 01-04, 03-settings-screen, 04-ota, 05-rpm]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Stack-based screen navigation with fixed-size array (no heap allocation)"
    - "lv_scr_load_anim() for animated screen transitions (slide left/right, 250ms)"
    - "lv_layer_top() for persistent UI elements across screen changes"
    - "Transition guard flag with one-shot LVGL timer to prevent navigation during animation"
    - "Loose coupling via IScreenManager interface pointer in StatusBar"

key-files:
  created:
    - "include/ui/screen_manager.h"
    - "src/ui/screen_manager.cpp"
  modified:
    - "include/interfaces/i_screen.h"
    - "include/ui/widgets/status_bar.h"
    - "src/ui/widgets/status_bar.cpp"

key-decisions:
  - "Fixed-size navigation stack array instead of std::vector (embedded, no heap allocation)"
  - "auto_del=false in lv_scr_load_anim() -- manual lifecycle management for predictable memory"
  - "Transition guard with +50ms timer buffer ensures LVGL animation completes before allowing next navigation"
  - "StatusBar uses IScreenManager* for loose coupling instead of direct ScreenManagerImpl reference"
  - "Menu button cycles JORNADA <-> NUMPAD for Phase 1, extensible for future screens"
  - "Back button hidden by default, shown only when navigation stack has entries"

patterns-established:
  - "All screen implementations must implement getLvScreen() returning their lv_obj_t*"
  - "Screen lifecycle: create on navigateTo, destroy on goBack, never destroy home screen"
  - "StatusBar buttons use static callback functions with user_data=this pattern"

# Metrics
duration: 5min
completed: 2026-02-09
---

# Phase 1 Plan 02: Screen Manager and StatusBar Navigation Summary

**Stack-based ScreenManagerImpl with lv_scr_load_anim() slide transitions and persistent StatusBar on lv_layer_top() with menu/back navigation buttons**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-09T21:20:36Z
- **Completed:** 2026-02-09T21:25:08Z
- **Tasks:** 2
- **Files created:** 2
- **Files modified:** 3

## Accomplishments

- Created ScreenManagerImpl implementing IScreenManager with push/pop navigation stack using fixed-size array (SCREEN_NAV_STACK_MAX=8)
- Implemented animated screen transitions: slide-left on push (navigateTo), slide-right on pop (goBack), 250ms duration
- Added transition guard flag with one-shot LVGL timer (+50ms buffer) to prevent navigation crashes during animation
- Extended IScreen interface with getLvScreen() method for LVGL screen object access
- Added OTA and RPM to ScreenType/screen_type_t enums for future phases
- Refactored StatusBar from per-screen parent to lv_layer_top() -- now persists across all screen transitions
- Added back button (LV_SYMBOL_LEFT) hidden by default, controlled by setBackVisible() API
- Added menu button (LV_SYMBOL_LIST) that cycles between JORNADA and NUMPAD screens
- Container is non-clickable (avoids blocking touch) but individual buttons are clickable
- All LVGL calls protected by bsp_display_lock/bsp_display_unlock

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement ScreenManagerImpl with stack navigation and animated transitions** - `ff55145` (feat)
2. **Task 2: Refactor StatusBar to lv_layer_top() with menu and back buttons** - `05cf7b9` (feat)

## Files Created/Modified

- `include/ui/screen_manager.h` - ScreenManagerImpl class declaration with singleton, stack navigation, transition guard
- `src/ui/screen_manager.cpp` - Full implementation: navigateTo (push+slide-left), goBack (pop+slide-right), showInitialScreen, C wrappers
- `include/interfaces/i_screen.h` - Added getLvScreen() to IScreen, OTA/RPM to ScreenType enum, updated C enum
- `include/ui/widgets/status_bar.h` - Refactored: no-parent create(), added backBtn_/menuBtn_, setBackVisible(), setScreenManager()
- `src/ui/widgets/status_bar.cpp` - Refactored: lv_layer_top() parent, menu/back button creation and callbacks, preserved existing functionality

## Decisions Made

- Fixed-size navigation stack array (no std::vector) -- avoids heap fragmentation on embedded ESP32-S3
- auto_del=false in lv_scr_load_anim() -- we manage screen lifecycle manually for predictable memory behavior
- Transition guard timer uses SCREEN_TRANSITION_TIME_MS + 50ms buffer to ensure LVGL animation fully completes
- StatusBar holds IScreenManager* (interface pointer) instead of ScreenManagerImpl* for loose coupling
- Menu button simple toggle (JORNADA <-> NUMPAD) for Phase 1, designed to be extensible
- Back button starts hidden (empty stack at boot), ScreenManager calls setBackVisible() on every push/pop

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added lvgl.h include to i_screen.h**
- **Found during:** Task 1
- **Issue:** getLvScreen() returns lv_obj_t* but i_screen.h did not include lvgl.h, causing compilation failure
- **Fix:** Added `#include "lvgl.h"` to i_screen.h
- **Files modified:** include/interfaces/i_screen.h

## Issues Encountered

None beyond the auto-fixed include.

## User Setup Required

None -- no external service configuration required.

## Next Phase Readiness

- ScreenManagerImpl is ready to receive JornadaScreen and NumpadScreen registrations (Plan 01-03)
- StatusBar is ready to be wired up with ScreenManager via setScreenManager() and setStatusBar()
- showInitialScreen() available for boot sequence to load first screen without animation
- The framework supports up to 8 levels of navigation stack depth (SCREEN_NAV_STACK_MAX)
- OTA and RPM screen type slots are reserved for Phase 4 and Phase 5

## Self-Check: PASSED

All files verified present, all commit hashes found in git log.

---
*Phase: 01-foundation*
*Completed: 2026-02-09*
