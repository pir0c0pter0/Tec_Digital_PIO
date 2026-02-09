# Research Summary: BLE + Screen Manager + OTA Milestone

**Domain:** Embedded ESP32-S3 firmware extension (BLE, screen navigation, OTA)
**Researched:** 2026-02-09
**Overall confidence:** MEDIUM (web search unavailable; training data verified against existing codebase)

## Executive Summary

Adding BLE communication, multi-screen navigation, and OTA firmware updates to the existing Teclado de Jornada Digital firmware is a well-trodden path in the ESP32-S3 ecosystem. The key technology choices are clear and unambiguous: NimBLE for BLE (not Bluedroid), the existing IScreen/IScreenManager interfaces for screen management (not LVGL fragments or tabview), and ESP-IDF's esp_ota_ops for OTA (not esp_https_ota, since there's no Wi-Fi).

The most consequential architectural decision is the new partition table. Moving from a single factory partition to dual OTA slots is a one-time, irreversible migration that erases all device data. This must happen before any devices reach the field. The 16MB flash provides ample space for two 3MB app slots, 1MB LittleFS, and NVS storage.

The primary technical risk is memory management. NimBLE adds ~50KB of internal SRAM usage to a system already running LVGL display buffers and audio decoding. The ESP32-S3 has PSRAM configured, and LVGL already uses custom malloc (which can leverage PSRAM), but NimBLE's buffers must remain in internal SRAM. Careful heap monitoring is essential during integration.

BLE OTA is the most complex feature. It requires careful state machine design, MTU negotiation for acceptable transfer speeds, disconnection handling, and -- critically -- rollback support to prevent field devices from being bricked. This should be the last feature implemented, after BLE core communication is proven stable.

## Key Findings

**Stack:** NimBLE (bundled in ESP-IDF 5.3.1) for BLE, custom ScreenManager on existing IScreen interface for navigation, esp_ota_ops for OTA, NVS for persistence. No external dependencies needed.

**Architecture:** Event queue pattern for BLE-to-UI communication (NimBLE callbacks cannot touch LVGL). Screen lifecycle with create/destroy on lv_obj_create(NULL) + lv_scr_load_anim(). BLE on Core 0 at priority 4 (below UI at priority 5, above nothing on Core 1 which is audio-dedicated).

**Critical pitfall:** OTA without rollback verification will brick field devices. Must implement self-test after OTA reboot with watchdog-triggered automatic rollback.

## Implications for Roadmap

Based on research, suggested phase structure:

1. **Phase 1: Foundation -- Partition Table + NVS + Screen Manager**
   - Addresses: Partition migration (do it first, before any field deployment), NVS persistence layer, ScreenManager implementation, button_manager.cpp decomposition
   - Avoids: Partition table migration pitfall (data loss); monolithic screen class anti-pattern
   - Rationale: Everything else depends on the new partition table (OTA needs dual slots) and screen manager (BLE status UI, OTA screen, settings screen all need it)

2. **Phase 2: BLE Core -- NimBLE + GATT Services**
   - Addresses: NimBLE init, LE Secure Connections, journey state notification, ignition notification, configuration read/write, BLE status in UI
   - Avoids: LVGL-from-BLE-callback crash pitfall; memory contention with audio
   - Rationale: BLE communication is the primary new capability. OTA depends on BLE being stable first.

3. **Phase 3: Settings + Configuration**
   - Addresses: Settings screen (volume, brightness, system params), BLE config sync, NVS persistence of settings
   - Avoids: Config sync race condition between local UI and BLE
   - Rationale: Settings screen exercises both screen manager and BLE. Good integration test before OTA complexity.

4. **Phase 4: OTA -- Firmware Update via BLE**
   - Addresses: OTA GATT service, OTA state machine, progress screen, rollback, MTU optimization
   - Avoids: Bricked devices pitfall; slow OTA without MTU negotiation; partial write on disconnect
   - Rationale: OTA is the most complex and risky feature. Do it last when BLE and screen manager are proven. A bug in OTA code can destroy devices.

5. **Phase 5: Polish -- Journey Logs, Documentation, Production Hardening**
   - Addresses: Journey history log download, BLE protocol documentation for app team, secure boot consideration, heap monitoring, stress testing
   - Rationale: Production readiness features after core functionality is stable.

**Phase ordering rationale:**
- Partition table MUST change before OTA code can even be tested (Phase 1 first)
- Screen manager MUST exist before settings screen or OTA screen can be built (Phase 1 first)
- BLE MUST be stable before OTA can be implemented over it (Phase 2 before Phase 4)
- Settings screen exercises both screen manager and BLE, serving as integration validation (Phase 3 between BLE and OTA)
- OTA is highest-risk and needs everything else working first (Phase 4 last in core features)

**Research flags for phases:**
- Phase 1: Standard patterns, unlikely to need research. IScreen interface already designed.
- Phase 2: May need deeper research on NimBLE GATT server API specifics. Verify CONFIG option names against actual ESP-IDF 5.3.1 menuconfig.
- Phase 3: Standard patterns, unlikely to need research.
- Phase 4: Likely needs deeper research on BLE OTA chunk size optimization, flow control, and resume-after-disconnect protocol design. This is the most novel code.
- Phase 5: May need research on secure boot v2 configuration if required for production.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack choices | HIGH | NimBLE for BLE, esp_ota_ops for OTA, NVS for persistence are the standard ESP32-S3 approach. No credible alternatives. |
| Features | HIGH | Feature list derived from existing PROJECT.md requirements + standard BLE device feature set. |
| Architecture | MEDIUM | Event queue and screen lifecycle patterns are well-established, but specific NimBLE callback signatures and esp_ota API details should be verified against docs. |
| Pitfalls | MEDIUM | Critical pitfalls (OTA rollback, LVGL thread safety, MTU) are well-known in the community. Specific CONFIG option names may need verification. |
| Partition layout | MEDIUM | Layout follows ESP-IDF partition table conventions, but exact offset alignment and size constraints should be verified with partition table tool. |
| Memory budget | LOW | ~70KB new RAM estimate is rough. Actual NimBLE + GATT overhead depends on number of characteristics, msys buffer config, and connection parameters. Must measure empirically. |

## Gaps to Address

- **NimBLE API verification:** Specific `ble_gap_*` and `ble_gatt_*` function signatures should be verified against ESP-IDF 5.3.1 headers during implementation. Training data may have minor inaccuracies.
- **BLE OTA throughput benchmarks:** Actual OTA transfer speed depends on negotiated MTU, connection interval, and BLE PHY. Need to measure empirically on target hardware.
- **Memory profiling:** Actual heap consumption after adding NimBLE needs measurement on real hardware. The ~50KB estimate for NimBLE is from training data.
- **iOS BLE behavior:** iOS has specific quirks with MTU negotiation (max ~185 bytes), GATT caching, and background BLE. These need testing with actual iOS devices.
- **Android BLE compatibility:** Android 8+ BLE stack behavior varies by manufacturer. Test with Samsung, Xiaomi, and Motorola devices (common in Brazilian market).
- **Partition table validation:** Run `gen_esp32part.py` to validate the proposed partition table before flashing.
- **BLE protocol documentation format:** The app team needs comprehensive BLE protocol documentation. Decide on format (markdown spec? Swagger-like BLE descriptor?) before Phase 2 ends.

## Files Created

| File | Purpose |
|------|---------|
| .planning/research/SUMMARY.md | This file -- executive summary with roadmap implications |
| .planning/research/STACK.md | Technology recommendations with versions and rationale |
| .planning/research/FEATURES.md | Feature landscape (table stakes, differentiators, anti-features) |
| .planning/research/ARCHITECTURE.md | System architecture, patterns, anti-patterns, file structure |
| .planning/research/PITFALLS.md | Critical, moderate, and minor pitfalls with prevention strategies |
