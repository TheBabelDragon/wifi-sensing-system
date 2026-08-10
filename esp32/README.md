# ESP32 CSI nodes (closed-loop)

| Direction | Port | Role |
|-----------|------|------|
| out | **4210** | CSI JSON broadcast → Echo Grid |
| in | **4211** | `echo_cmd` from Echo Grid |

## Boot policy

**Sensing first. Same local path for virgin and provisioned nodes.**

1. `WIFI_STA` + hardware CSI start immediately  
2. If not associated → lock **channel 6** (stable promiscuous CSI)  
3. If NVS has saved STA creds → background join home AP  
4. If virgin (no creds) → stay local; **no auto portal**  
5. Portal only when you ask: serial `portal`  
6. UDP to Echo only while associated  

Provisioned vs virgin no longer differ by “stuck in portal / scan hop.”  
They only differ by uplink (UDP) and AP channel once joined.

## Serial commands

```
portal   # open WiFiManager AP once, save home Wi‑Fi
status   # wifi/creds/csi/channel/packets
```

## Boards

| Flag | Board |
|------|--------|
| `--standard` | ESP32 DevKit |
| `--cyd` | Cheap Yellow Display |
| `--s3` | ESP32-S3 |

## Flash

```bash
cd wifi-sensing-system/esp32
git pull
chmod +x flash.sh

./flash.sh --cyd -p /dev/ttyUSB0 -e --monitor
./flash.sh --cyd --node esp32_cyd_02 -p /dev/ttyUSB1 -e
./flash.sh --standard --node esp32_node_02 -p /dev/ttyUSB1 -e
./flash.sh --s3 --node esp32_s3_01 -p /dev/ttyACM0 -e
```

## First credentials (virgin node)

1. Flash and boot — CSI UI already live  
2. Serial monitor: type `portal`  
3. Join `ESP32-CSI-<node>`, set home Wi‑Fi  
4. Same LAN as Echo host for UDP  

## Echo Grid

```bash
python visualization/dashboard.py --csi
```
