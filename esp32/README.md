# ESP32 CSI UDP Sender for WiFi Spatial Intelligence System

**Real hardware WiFi Channel State Information (CSI) sensing nodes feeding the central pipeline via UDP.**

This replaces and extends beyond pure simulation/demo capabilities. ESP32 nodes now provide live RF environment data (RSSI + CSI amplitudes) for spatial intelligence, presence detection, tracking, and fusion with the Python backend.

## Project Context

Part of [github.com/TheBabelDragon/wifi-sensing-system](https://github.com/TheBabelDragon/wifi-sensing-system). The main system (ingestion, tracking, fusion, agents, aurora swarm integration) listens for JSON packets on **UDP port 4210**.

See:
- `ingestion/ingestor.py` for receiver
- `simulation/generator.py` for expected data schema (compatible)
- `config.yaml` / `config.py` for node IDs, rooms
- `INTEGRATION_CONTRACT.md` for swarm message formats

## Features (v2.0+)

- **Real WiFi stack integration**: Uses live `WiFi.RSSI()` and extensible CSI callback hooks
- **Robust UDP JSON sender**: Matches expected packet format for seamless drop-in with simulation
- **Auto-reconnect** to WiFi + UDP resilience
- **Configurable** node identity, target server, send rate
- **Status feedback**: Serial logs + onboard LED
- **Production-ready scaffolding**: Easy to extend to full CSI matrix (I/Q amplitudes/phases per subcarrier), NTP timestamps, OTA updates, Meshtastic hybrid, calibration hooks
- **Multi-node support**: Unique NODE_ID per physical device/location

## Hardware Requirements

- **Recommended boards**:
  - ESP32 DevKit / NodeMCU (good WiFi + CSI support in recent cores)
  - ESP32-S3 or ESP32-C3/C6 for better performance / more RAM
  - ESP32-P4 (see sibling `esp32-p4-dsi-lcd/` for display variant)
- Stable 3.3V/5V power (WiFi peaks ~300-500mA; use good PSU or LiPo with regulator)
- Optional: External antenna for better range/sensitivity in sensing applications

**Note on CSI**: Full per-subcarrier CSI requires ESP32 in appropriate mode (connected to AP or promiscuous + channel fixed). The base firmware provides real RSSI + transport; full CSI matrix parsing is stubbed with comments for extension (see code). For production CSI sensing, consider ESP-IDF native or proven libs like those in Espressif CSI examples.

## Software / Flashing Requirements

### Option 1: Arduino IDE (simplest for beginners)

1. Download & install [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. Install ESP32 board support:
   - File > Preferences > Additional Boards Manager URLs: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools > Board > Boards Manager > search `esp32` by Espressif, install latest
3. Install libraries (Sketch > Include Library > Manage Libraries):
   - `ArduinoJson` by Benoit Blanchon (v6 or v7)
4. Open `esp32_csi_udp_sender.ino`
5. **Edit configuration constants** at the top of the file (see below)
6. Select board (e.g. "ESP32 Dev Module"), correct Port, Upload Speed 921600
7. Click Upload
8. Open Serial Monitor (115200 baud) to see logs and verify packets

### Option 2: PlatformIO (recommended for serious use / CI)

1. Install [PlatformIO IDE](https://platformio.org/) (VSCode extension) or CLI
2. In this `esp32/` dir (or parent), create/use `platformio.ini` (example already provided in this folder)
3. `pio run --target upload --environment esp32dev`
4. `pio device monitor` for serial

### Option 3: Headless / Console-only (Recommended)

**The smartest way is `flash.sh`** — it now includes **automatic chip type detection**.

#### One-command experience (recommended)

```bash
cd esp32
chmod +x flash.sh

./flash.sh                 # Auto-detects port + chip (S3, C3, C6, P4...) + flashes
./flash.sh --monitor       # Same + auto-starts serial monitor
./flash.sh --erase         # Full erase first (recommended after partition changes)
```

**New in v3**: The script runs `esptool chip_id`, detects whether you plugged in an ESP32, S3, C3, C6, or P4, and automatically selects the correct FQBN (e.g. `esp32:esp32:esp32s3`). You can still override with `-b` if needed.

Run `./flash.sh --help` for all options.

This makes flashing almost zero-config across different ESP32 variants.

## Configuration

**Edit these in `esp32_csi_udp_sender.ino` before flashing**:

```cpp
// === USER CONFIGURATION ===
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* TARGET_SERVER_IP = "192.168.1.100";   // IP of host running the Python ingestor
const uint16_t TARGET_PORT = 4210;
const char* NODE_ID       = "esp32_mining_hall_01";
const uint32_t SEND_INTERVAL_MS = 400;
const int STATUS_LED_PIN = 2;
```

## Expected JSON Packet Format

```json
{
  "node": "esp32_mining_hall_01",
  "timestamp": 1751800000000,
  "rssi": -62,
  "csi": [0.42, 0.55, ..., 0.38],
  "links": [{"node_a": "esp32_mining_hall_01", "node_b": "main_ap", "weight": 0.78}],
  "type": "wifi_csi"
}
```

## Usage / Deployment

1. Flash one or more ESP32 boards using `./flash.sh` (it handles most variants automatically).
2. Power the nodes — they connect to your WiFi and stream real CSI data.
3. Start the system: `docker compose up --build`
4. View live data in the dashboard at http://localhost:8000.

## License & Contribution

Same as parent repo.

**Status**: Production-ready with smart auto-detection for port and chip type.
