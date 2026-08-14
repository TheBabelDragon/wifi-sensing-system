# ESP32 CSI nodes (ESP-NOW + auto WiFiManager)

| Path | Role |
|------|------|
| **ESP-NOW** | Compact CSI broadcast — always on, all nodes |
| **UDP :4210** | Full CSI JSON → Echo Grid (when on house WiFi) |
| **UDP :4211** | `echo_cmd` from Echo Grid |

## Boot behaviour

1. CSI + ESP-NOW start  
2. **WiFiManager portal opens automatically** (AP `ESP32-CSI-<node>`, 3 min timeout)  
3. Join that AP on your phone → set house WiFi  
4. After portal (success or timeout) → CSI + ESP-NOW resume  
5. No serial command required for first setup  

ESP-NOW keeps working whether or not the node is on house WiFi.  
Any node that *is* on WiFi bridges peer ESP-NOW traffic to UDP.

## Serial (optional)

```
status   # wifi / espnow / packet counters
portal   # open WiFiManager again
```

## Boards

| Flag / Env | Board |
|------------|--------|
| `--standard` / `esp32-standard` | ESP32 DevKit |
| `--cyd` / `esp32-cyd` | Cheap Yellow Display |
| `--s3` / `esp32-s3` | ESP32-S3 |

## Flash

```bash
cd wifi-sensing-system/esp32
git pull
chmod +x flash.sh

./flash.sh --standard -p /dev/ttyUSB0 -e --monitor
./flash.sh --cyd      -p /dev/ttyUSB1 -e --monitor
./flash.sh --s3       -p /dev/ttyACM0  -e --monitor
```

### Pure PlatformIO

```bash
pio run -e esp32-standard -t upload --upload-port /dev/ttyUSB0
pio run -e esp32-cyd -t upload --upload-port /dev/ttyUSB1 \
  --build-flag '-DNODE_ID_STR="esp32_cyd_02"'
```

## First setup (no serial typing needed)

1. Flash the board  
2. On your phone, join **`ESP32-CSI-<node>`**  
3. Captive portal → enter house WiFi  
4. Node joins LAN and starts UDP CSI; ESP-NOW peers are bridged automatically  

If you skip the portal it times out after 3 minutes and keeps running offline on ESP-NOW only.

## Echo Grid

```bash
python visualization/dashboard.py --csi
```

Listens on UDP **4210**. One WiFi-connected node is enough to bridge the whole ESP-NOW swarm.
