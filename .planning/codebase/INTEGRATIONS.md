# External Integrations

**Analysis Date:** 2026-02-09

## Hardware Peripherals

**QSPI LCD Display (AXS15231B):**
- Device: 3.5" TFT LCD with AXS15231B controller (320×480 native, rotated 90° → 480×320 working)
- Interface: QSPI (Quad SPI) — 4-bit parallel high-speed data
- Pins: CS=45, SCK=47, D0-D3=21/48/40/39, TE=38, BLK=1
- Driver: `src/esp_lcd_axs15231b.c` (custom Espressif driver)
- Configuration: Initialized in `src/esp_bsp.c` via `bsp_display_init()`
- SPI Bus: QSPI host with DMA enabled (`SPI_DMA_CH_AUTO`)
- Backlight: GPIO1, active-high control

**Capacitive Touch Controller (I2C):**
- Interface: I2C (Two-Wire) — I2C address typically 0x38 or 0x48 (hardware-dependent)
- Pins: SCL=8, SDA=4, INT=3 (interrupt signal)
- Driver: `src/esp_lcd_touch.c` (generic capacitive touch driver)
- Configuration: Initialized in `src/esp_bsp.c` via `bsp_touch_init()`
- Callback: `esp_lcd_touch_interrupt_callback_t` for touch events
- Max points: 1 (single-touch only)

**Audio Codec (I2S Output):**
- Output: 16-bit PCM audio via I2S peripheral (I2S_NUM_0)
- Pins: BCK (Bit Clock)=42, LRCK (Left/Right Clock)=2, DOUT (Data Out)=41
- Sample rate: 44.1 kHz, 48 kHz (mono or stereo, configurable per stream)
- Audio decoding: MP3 → PCM via minimp3 library
- Amplification: External I2S amplifier required (not onboard)
- Configuration: `src/simple_audio_manager.cpp` initializes I2S channel with `I2S_STD_CLK_DEFAULT_CONFIG(44100)`

**Ignition Signal Input (GPIO):**
- Pin: GPIO18 (digital input)
- Signal: Ignition on/off detection (vehicle ignition switch)
- Debounce: Dual-threshold software debounce
  - ON transition: 1.0 seconds confirmation (`IGNICAO_DEBOUNCE_ON_S`)
  - OFF transition: 2.0 seconds confirmation (`IGNICAO_DEBOUNCE_OFF_S`)
- Check interval: 100 ms polling (`IGNICAO_CHECK_INTERVAL`)
- Callback: Registered via ignition service (`src/services/ignicao/ignicao_service.h`)

**Battery Voltage Monitor (ADC):**
- Pin: GPIO5 (ADC input)
- Purpose: Monitor battery/power supply voltage
- Not actively integrated in current codebase (pin defined but not used in main loop)

## Data Storage

**Flash Filesystem (LittleFS):**
- Partition: `spiffs` (1 MB, 0x310000–0x3FFFFF)
- Mount point: `/littlefs` (configured in `include/config/app_config.h`)
- Files: 11 MP3 audio files (~400 KB total) + 1 PNG splash image (~12 KB)
  - `ign_on_jornada_manobra.mp3` — Ignition on/journey start
  - `ign_off.mp3` — Ignition off signal
  - `click.mp3` — UI button click feedback
  - `ok_click.mp3`, `nok_click.mp3` — Positive/negative action sounds
  - `ok_user.mp3` (not listed but referenced) — User acknowledgment
  - `nok_user.mp3` — User error/rejection
  - `identificacao_ok.mp3` — Successful identification
  - `digite_senha.mp3` — Prompt for password entry
  - `aproxime_rfid.mp3` — RFID card prompt
  - `alerta_max_rpm.mp3`, `alerta_max_vel.mp3` — Engine/speed alerts
  - `logo_splash.png` — Splash screen (280×80, RGBA)
- Driver: LittleFS via `joltwallet/littlefs@1.14.8` (ESP-IDF component)
- LVGL Bridge: `src/lvgl_fs_driver.cpp` maps LVGL file API to LittleFS (drive 'A:/')
- Access: LVGL can load PNG images directly; audio manager reads MP3s via standard file I/O

**NVS (Non-Volatile Storage):**
- Partition: `nvs` (20 KB, 0x9000–0xDFFF)
- Purpose: Store persistent key-value pairs (Wi-Fi credentials, driver profiles, etc.)
- Not actively used in current driver state machine (future feature)

## Communication Buses

**QSPI Bus (LCD Display):**
```
SPI Host: QSPI_HOST (high-speed LCD interface)
Speed: 80 MHz
DMA: Enabled (SPI_DMA_CH_AUTO)
Mode: Master
```
Handles: `esp_lcd_panel_handle_t` (created by esp_lcd driver stack)

**I2C Bus (Touch, Future Expansion):**
```
Port: I2C_NUM_0 (default)
Speed: 100 kHz or 400 kHz (standard/fast mode, configured in esp_bsp.c)
SDA: GPIO4
SCL: GPIO8
```
Driver: ESP-IDF `driver/i2c.h` via `i2c_driver_install()`, managed in `bsp_i2c_init()`

**I2S Audio Bus:**
```
Port: I2S_NUM_0
Role: Master (ESP32-S3 drives clock)
Mode: Standard (MSB-aligned, 16-bit)
Slot: Mono or Stereo (configurable, default mono)
Clock: 44.1 kHz (default for MP3), 48 kHz supported
Flags: Auto-clear enabled
```
Driver: ESP-IDF `driver/i2s_std.h` via `i2s_new_channel()`, initialized in `src/simple_audio_manager.cpp`

## Audio Assets & Streaming

**MP3 Storage & Playback Pipeline:**
```
LittleFS (/littlefs/*.mp3)
  ↓
Audio Queue (4 slots for pending requests)
  ↓
Audio Task (FreeRTOS, Core 1, priority 5)
  ↓
minimp3 Decoder (frame-by-frame MP3 → PCM)
  ↓
I2S Driver (16-bit, 44.1/48 kHz)
  ↓
DAC/Amplifier → Speaker
```

**Configuration in `include/config/app_config.h`:**
```c
#define AUDIO_FILE_IGN_ON       "/ign_on_jornada_manobra.mp3"
#define AUDIO_FILE_IGN_OFF      "/ign_off.mp3"
#define AUDIO_FILE_CLICK        "/click.mp3"
// ... 8 more file paths
#define AUDIO_BUFFER_SIZE       (8 * 1024)   // MP3 read buffer
#define AUDIO_QUEUE_SIZE        4            // Pending play requests
#define AUDIO_I2S_TIMEOUT_MS    100          // I2S write timeout
#define AUDIO_VOLUME_MIN        0
#define AUDIO_VOLUME_MAX        21
#define AUDIO_VOLUME_DEFAULT    21           // Max volume on startup
```

**Audio Manager API:**
- `initSimpleAudio()` — Initialize I2S, allocate buffers
- `playAudioFile(const char* filename)` — Queue MP3 file for playback
- `stopAudio()` — Stop current playback immediately
- `isAudioPlaying()` — Check if audio is active
- `setAudioVolume(int volume)` — Set volume 0–21

## Task Scheduling & Threading

**FreeRTOS Task Distribution:**

**Core 0 (UI/System):**
- `system_task` (priority 5) — 5 ms tick, main event loop, LVGL rendering
  - Polls ignition status every 100 ms (IGNICAO_CHECK_INTERVAL)
  - Updates status bar every 250 ms
  - Handles button events, navigation, popups
- Mutexes protect shared state with Core 1

**Core 1 (Audio):**
- `audio_task` (priority 5) — Dedicated MP3 decoding and I2S streaming
  - Polls audio queue for play requests
  - Decodes MP3 frames using minimp3
  - Writes PCM samples to I2S at 44.1/48 kHz

**Synchronization:**
- `audioMutex` — Guards audio state (`is_playing`, `volume`, `stop_requested`)
- `display_lock_timeout` (100 ms) — LVGL thread-safety

## Ignition & Journey State Machine

**Ignition Service (`src/services/ignicao/`):**
- Monitors GPIO18 for vehicle ignition state
- Dual debounce prevents false transitions
- Triggers audio feedback via `AUDIO_FILE_IGN_ON` and `AUDIO_FILE_IGN_OFF`
- Callback: `onIgnicaoStatusChange(bool newStatus)` in `src/main.cpp`

**Journey Service (`src/services/jornada/`):**
- Manages driver journey state (7 states: INATIVO, JORNADA, MANOBRA, REFEICAO, ESPERA, DESCARGA, ABASTECIMENTO)
- Up to 3 drivers with driver rotation
- State transitions via button taps
- Integrated with ignition status (journey only active when ignition ON)

## Networking

**Current Status:** Not implemented

**Future Capability:**
- NVS partition exists (20 KB) for potential Wi-Fi/Bluetooth credentials
- No Wi-Fi or Bluetooth drivers currently enabled
- I2C bus available for future expansion (RTC, sensors, etc.)

## External Services

**None currently integrated.** Project is fully offline/standalone:
- No cloud connectivity
- No GPS/GNSS
- No external APIs
- No OTA (Over-The-Air) firmware updates configured
- All state stored locally in NVS + journey logs (future feature)

## Partition Layout

```
Partition Table (partitions.csv):
0x000000 - 0x008FFF (36 KB)     : nvs      (key-value storage)
0x009000 - 0x00DFFF (20 KB)     : phy_init (PHY calibration)
0x010000 - 0x30FFFF (3 MB)      : app0     (factory app, main firmware)
0x310000 - 0x40FFFF (1 MB)      : spiffs   (LittleFS filesystem)
```

No OTA or secondary app partition configured.

---

*Integration audit: 2026-02-09*
