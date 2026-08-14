# ESP32 CSI nodes (ESP-NOW + auto WiFiManager)

| Path | Role |
|------|------|
| **ESP-NOW** | Compact CSI broadcast — always on |
| **UDP :4210** | Full CSI JSON → Echo Grid (when on house WiFi) |
| **UDP :4211** | `echo_cmd` from Echo Grid |

## Node IDs (unique per board)

Each board names itself from its **MAC address**:

| Board | Example ID | Portal AP |
|-------|------------|-----------|
| DevKit | `csi-A1B2C3` | `CSI-csi-A1B2C3` |
| CYD | `cyd-A1B2C3` | `CSI-cyd-A1B2C3` |

No more shared `node_01` / `cyd_01` names. Every board is unique on your phone.

## Boot behaviour

1. Read MAC → unique `NODE_ID`  
2. If saved WiFi exists → try join (~8 s)  
3. If not connected → **WiFiManager portal** (AP `CSI-<nodeid>`, 3 min)  
4. Then CSI + ESP-NOW start  
5. ESP-NOW works with or without house WiFi  

## Flash

```bash
cd wifi-sensing-system/esp32
git pull
chmod +x flash.sh

# Standard DevKit
./flash.sh --standard -p /dev/ttyUSB0 -e --monitor

# Cheap Yellow Display
./flash.sh --cyd -p /dev/ttyUSB1 -e --monitor
```

### Pure PlatformIO

```bash
pio run -e esp32-standard -t upload --upload-port /dev/ttyUSB0
pio run -e esp32-cyd -t upload --upload-port /dev/ttyUSB1
```

## First setup

1. Flash  
2. On phone, join **`CSI-csi-XXXXXX`** or **`CSI-cyd-XXXXXX`**  
3. Enter house WiFi  
4. Done — node is unique and on the LAN  

Optional serial: `status` | `portal`

## Echo Grid

```bash
python visualization/dashboard.py --csi
```

UDP **4210**. One WiFi-connected node bridges ESP-NOW peers automatically.
