# ESP32 CSI UDP Sender

## Cheap Yellow Display (ESP32-2432S028) — 2.8" Resistive Touch

**File:** `esp32_csi_cyd.ino`

### PlatformIO (Recommended)

Add or use this environment in `platformio.ini`:

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
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git

build_flags =
    -D USER_SETUP_LOADED
    -D ILI9341_DRIVER
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

Then build:
```bash
pio run --target upload --environment esp32-2432S028-cyd
```

**Note:** We use the GitHub URL for the touchscreen library because the PlatformIO registry version can sometimes cause "Unknown package" errors.

### What it does
- WiFiManager configuration
- Real CSI collection
- Nice 2.8" display with status + live CSI bar graph
- Sends data to your central pipeline
