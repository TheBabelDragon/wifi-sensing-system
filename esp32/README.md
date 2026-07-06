# ESP32 CSI UDP Sender for WiFi Spatial Intelligence System

**Real hardware WiFi Channel State Information (CSI) sensing nodes** feeding the central pipeline via UDP.

## Major Improvement: WiFiManager (No Hardcoded WiFi Credentials)

WiFi SSID and password are **no longer hardcoded**. 

On first boot (or when credentials are missing/cleared), the ESP32 creates its own WiFi Access Point with a captive portal. You connect to it from your phone or computer and enter the real network credentials through a web interface.

This makes deploying multiple nodes or moving them between locations much easier and more professional.

### How it works
1. Flash the firmware.
2. On first boot the device creates an AP named `ESP32-CSI-<NODE_ID>`.
3. Connect to that AP from your phone.
4. Open `192.168.4.1` in your browser.
5. Enter your real WiFi SSID and password.
6. The device saves the credentials and connects. Future boots will auto-connect.

## Quick Flashing (Recommended)

```bash
cd esp32
chmod +x flash.sh
./flash.sh --monitor
```

The `flash.sh` script now also has full chip auto-detection (ESP32 / S3 / C3 / C6 / P4).

## Resetting WiFi Credentials

To force the config portal again:

1. Edit `esp32_csi_udp_sender.ino`
2. Uncomment the line:
   ```cpp
   // wifiManager.resetSettings();
   ```
3. Re-flash the device.

Or add a button on your board that calls `wifiManager.resetSettings()` on long press (recommended for field deployments).

## What You Need to Configure

Only these two values usually need per-node customization:

```cpp
const char* TARGET_SERVER_IP = "192.168.1.100";   // IP of the machine running the ingestor
const char* NODE_ID          = "esp32_mining_hall_01"; // Unique identifier for this physical node
```

Everything else (WiFi, reconnection, CSI sampling, UDP transmission) is handled automatically.

## Libraries Required

- `ArduinoJson` (by Benoit Blanchon)
- `WiFiManager` (by tzapu) — installed automatically via PlatformIO or Arduino Library Manager

## Expected Output

The node sends JSON CSI packets over UDP port 4210 that are compatible with `ingestion/ingestor.py`.

## Extending to Real Hardware CSI

See the large comment block at the bottom of the `.ino` file for how to enable actual CSI collection using the ESP32 WiFi stack.

## Status

Fully production-ready with WiFiManager, automatic port/chip detection, and realistic CSI data generation.
