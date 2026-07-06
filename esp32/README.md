# ESP32 CSI UDP Sender

## Supported Boards

### 1. Standard ESP32 Dev Boards (Recommended for beginners)
Use `esp32_csi_udp_sender.ino`

### 2. Cheap Yellow Display (ESP32-2432S028) — 2.8" 320x240 Resistive Touch

This is one of the most popular cheap ESP32 display boards.

**File to use:** `esp32_csi_cyd.ino`

#### Flashing Instructions for Cheap Yellow Display (CYD)

**Recommended: PlatformIO (easiest and most reliable)**

1. Install PlatformIO in VS Code
2. Open the `esp32/` folder
3. Add this environment to your `platformio.ini`:

```ini
[env:esp32-2432S028-cyd]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    bblanchon/ArduinoJson @ ^6.21.5
    tzapu/WiFiManager @ ^2.0.17
    bodmer/TFT_eSPI @ ^2.5.43
    paulstoffregen/XPT2046_Touchscreen @ ^1.4

build_flags =
    -D USER_SETUP_LOADED=1
    -D ILI9341_DRIVER=1
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=320
    -D TFT_MISO=12
    -D TFT_MOSI=13
    -D TFT_SCLK=14
    -D TFT_CS=15
    -D TFT_DC=2
    -D TFT_RST=-1
    -D TFT_BL=21
    -D TOUCH_CS=33
    -D SPI_FREQUENCY=40000000
    -D SPI_READ_FREQUENCY=20000000
```

4. Build and upload:
```bash
pio run --target upload --environment esp32-2432S028-cyd
```

**Alternative: Arduino IDE**

1. Install these libraries:
   - `TFT_eSPI` by Bodmer
   - `XPT2046_Touchscreen` by Paul Stoffregen
   - `WiFiManager` + `ArduinoJson`
2. In `TFT_eSPI/User_Setup.h`, use the settings for the **ESP32-2432S028** (many ready-made setups exist on GitHub).
3. Upload `esp32_csi_cyd.ino`

#### What the CYD version does
- Uses WiFiManager for easy WiFi configuration
- Enables real CSI collection
- Shows live status on the 2.8" display:
  - WiFi status + RSSI
  - Whether it's using Real CSI or Simulation
  - Packet counter
  - Simple real-time CSI bar graph visualization
- Still sends JSON CSI data over UDP to your central pipeline

This turns the Cheap Yellow Display into a nice visual CSI sensing node.

## General Notes

- All versions support **WiFiManager** (no hardcoded credentials)
- Real CSI is enabled by default on boards that support it well
- Use `./flash.sh` for standard boards (auto chip + port detection)
