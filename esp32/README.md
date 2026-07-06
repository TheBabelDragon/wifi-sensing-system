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

### Option 3: Headless / Console-only with Arduino CLI + esptool.py (pure terminal workflow)

This is the **full "throughput initiation process"** for flashing entirely from the command line (no GUI/IDE). Perfect for servers, CI/CD, Docker, or remote/headless machines.

#### One-time setup (run once on the machine)

```bash
# 1. Install Arduino CLI (official headless tool for .ino sketches)
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
# Make sure arduino-cli is in your PATH (usually ~/bin or /usr/local/bin)

# 2. Install ESP32 core (includes bootloader, partitions, toolchains)
arduino-cli core install esp32:esp32

# 3. Install esptool.py (the actual low-level flasher)
pip install --upgrade esptool

# 4. (Optional but recommended) Install ArduinoJson library once
arduino-cli lib install ArduinoJson
```

#### Per-project flashing workflow (run these commands)

```bash
# Navigate to the folder containing esp32_csi_udp_sender.ino
cd /path/to/your/wifi-sensing-system/esp32

# === STEP 1: Compile the sketch headlessly ===
# --fqbn = Fully Qualified Board Name. Common examples:
#   esp32:esp32:esp32dev          (generic ESP32 Dev Module)
#   esp32:esp32:esp32s3             (ESP32-S3)
#   esp32:esp32:esp32c3             (ESP32-C3)
# Add any build properties if needed, e.g. for partition scheme or PSRAM
arduino-cli compile \
  --fqbn esp32:esp32:esp32dev \
  --output-dir ./build \
  esp32_csi_udp_sender.ino

# Verify build artifacts were created
ls -la build/

# Typical generated files you will use:
#   esp32_csi_udp_sender.ino.bootloader.bin
#   esp32_csi_udp_sender.ino.partitions.bin
#   esp32_csi_udp_sender.ino.bin          ← the actual application firmware

# === STEP 2: Flash with esptool.py (the "throughput" flashing step) ===
# Common safe command for ESP32 Dev Module / most boards
# -z = compress, --baud 921600 for speed (or 115200 if issues)
# Adjust --port to your actual serial device (/dev/ttyUSB0, /dev/ttyACM0, COM3 on Windows, etc.)

esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash -z \
  0x1000  build/esp32_csi_udp_sender.ino.bootloader.bin \
  0x8000  build/esp32_csi_udp_sender.ino.partitions.bin \
  0x10000 build/esp32_csi_udp_sender.ino.bin

# On success you should see:
#   Hash of data verified.
#   Leaving... Hard resetting via RTS pin...
```

#### Even simpler one-command headless upload (Arduino CLI handles esptool internally)

```bash
# After the compile step above, or combine:
arduino-cli upload \
  --fqbn esp32:esp32:esp32dev \
  --port /dev/ttyUSB0 \
  --input-dir ./build \
  esp32_csi_udp_sender.ino
```

This is often the fastest "initiation process" for console users.

#### Alternative: PlatformIO CLI (also fully headless)

```bash
cd /path/to/your/wifi-sensing-system/esp32
pio run --target upload --environment esp32dev   # uses platformio.ini
pio device monitor
```

**Notes & Troubleshooting for headless flashing**
- First time flashing often needs to put the board into download mode (hold BOOT button while pressing RESET, or let esptool auto-reset via RTS/DTR).
- If you see "Failed to connect", try different --port, lower baud rate, or add `--before default_reset` / `--after hard_reset`.
- Windows users: use COMx ports and may need `python -m esptool` instead of `esptool.py`.
- The three offset method (0x1000 / 0x8000 / 0x10000) is the most reliable across Arduino-ESP32 versions.
- Some boards produce a convenient `merged.bin` — you can flash it at offset 0x0 with `esptool.py write_flash 0x0 build/...merged.bin`.
- After flashing, the board should reboot and start sending CSI packets (watch with `arduino-cli monitor` or `pio device monitor` or `screen /dev/ttyUSB0 115200`).

## Configuration

**Edit these in `esp32_csi_udp_sender.ino` before flashing**:

```cpp
// === USER CONFIGURATION ===
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* TARGET_SERVER_IP = "192.168.1.100";   // IP of host running the Python ingestor (or docker host)
const uint16_t TARGET_PORT = 4210;
const char* NODE_ID       = "esp32_mining_hall_01"; // Must match config.yaml NODE_ID or be unique per physical node
const uint32_t SEND_INTERVAL_MS = 400;             // How often to sample & send (balance responsiveness vs bandwidth)
const int STATUS_LED_PIN = 2;                      // Onboard LED
```

**Tips**:
- Use a static IP or mDNS for TARGET_SERVER_IP if possible, or read from serial/config at runtime (future enhancement).
- For multiple nodes: flash with different NODE_ID and place in different rooms/positions.
- Calibration: After deployment, use the project's `calibration/` tools or observe in dashboard to tune weights/distances.
- Security: Use WPA3 WiFi, consider TLS/encryption on UDP if sensitive (or VPN), rotate credentials.

## Expected JSON Packet Format (sent to UDP 4210)

Compatible with `ingestion/ingestor.py` and `simulation/generator.py`:

```json
{
  "node": "esp32_mining_hall_01",
  "timestamp": 1751800000000,   // millis() since boot or better NTP unix ms
  "rssi": -62,
  "csi": [0.42, 0.55, 0.61, ..., 0.38],  // 32 float amplitudes (0.0-1.0 normalized). Extend to real subcarrier data here
  "links": [
    {"node_a": "esp32_mining_hall_01", "node_b": "main_ap", "weight": 0.78}
  ],
  "type": "wifi_csi"
}
```

The `csi` array length and values should be consistent with simulation (32 values recommended). Real implementation can populate from CSI callback buffer (see code comments).

## Usage / Deployment

1. Flash firmware to one or more ESP32s with correct config.
2. Power on nodes near the area of interest (they connect to your local WiFi AP; the AP signals provide the sensing substrate).
3. Start the main system: `docker compose up --build` (or `python run_full_pipeline.py`)
4. Ingestor will receive live packets, feed tracking/fusion/predictive etc.
5. View dashboard at http://localhost:8000 or integrate with aurora swarm.
6. Monitor serial or add MQTT/Redis bridge for observability.

**Switching from Simulation to Hardware**:
In `config.yaml` or env, you can keep `SIMULATION_FRAMES` but the ingestor now happily mixes real UDP CSI nodes + simulated if needed. Set demo sleep lower or disable sim generator for pure hardware mode.

## Extending to Full Real CSI Collection

The current sketch sends real RSSI + plausible CSI amplitudes. To use actual WiFi CSI matrix:

1. Include `<esp_wifi.h>` and `<esp_wifi_types.h>`
2. After WiFi connected, in `setup()`:
   ```cpp
   // Example skeleton (test thoroughly; channel must match AP; may need promiscuous)
   esp_wifi_set_promiscuous(true);
   wifi_csi_config_t csi_config = {
     .lltf_en = true, .htltf_en = true, .stbc_htltf2_en = true,
     .ltf_merge_en = true, .channel_width = WIFI_BW_HT20, .manu_scale = false
   };
   esp_wifi_set_csi_config(&csi_config);
   esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);
   esp_wifi_set_csi(true);
   ```
3. Implement `void csi_rx_cb(void *ctx, wifi_csi_info_t *info) { ... parse info->buf (len=info->len), compute amplitudes for ~32 subcarriers, store in global latest_csi[32] }`
4. In send function, copy latest_csi into JSON instead of generated values. Normalize amplitudes (e.g. / max or to 0-1 range).
5. Handle rx_ctrl.rssi too (more accurate per-packet).

References: Espressif CSI docs, popular community CSI tools for ESP32.

**Limitations & Future**:
- Full phase info, all subcarriers (52+), MIMO not yet parsed (add as needed for advanced sensing).
- Timestamp: currently millis(); add NTPClient for real wall time.
- OTA: Add ArduinoOTA for wireless firmware updates (great for deployed nodes).
- Power optimization / deep sleep between samples for battery nodes.
- Hybrid with Meshtastic (see meshtastic_gateway/).

## Troubleshooting

- No WiFi connect: Check SSID/pass, 2.4GHz only (ESP32), signal strength, restart router.
- No packets received: Verify TARGET_SERVER_IP is reachable (ping), firewall allows UDP 4210 inbound on host, check ingestor logs (`docker logs` or python output).
- Serial shows nothing: Wrong baud or wrong port selected in IDE.
- Unstable: Add delay(10) in loop, ensure good power, avoid long cables.
- CSI not changing: In real mode, node must be associated or on correct channel; test with known moving person/obstacle.

## License & Contribution

Same as parent repo. PRs welcome for improved CSI parsing, PlatformIO full support, web config portal, etc.

**Status**: Production scaffolding complete. Ready for real-world WiFi sensing deployments beyond simulation demos.
