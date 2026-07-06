# ESP32 CSI UDP Sender

## Recommended Approach (Unified Firmware)

We now use a **single unified sketch** (`esp32_csi_unified.ino`) that supports both:

- Standard ESP32 boards (WROOM-32UE, DevKit, etc.)
- Cheap Yellow Display (ESP32-2432S028)

### How to choose the board type

**Using the smart flasher script (recommended):**

```bash
./flash.sh --standard          # Normal ESP32
./flash.sh --cyd               # Cheap Yellow Display
./flash.sh --cyd --monitor     # CYD + auto start serial monitor
```

**Using PlatformIO directly:**

```bash
# Standard ESP32
pio run --target upload --environment esp32-standard

# Cheap Yellow Display
pio run --target upload --environment esp32-cyd
```

The only difference is controlled by the `HAS_DISPLAY` build flag. No need to maintain two separate sketches anymore.

## Compilation Check

Both environments compile cleanly:
- `esp32-standard` → No display code included
- `esp32-cyd` → Full TFT + touch support included

You can safely switch between boards without code changes.
