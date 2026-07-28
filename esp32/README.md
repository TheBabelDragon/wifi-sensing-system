# ESP32 CSI nodes (closed-loop)

| Direction | Port | Role |
|-----------|------|------|
| out | **4210** | CSI JSON broadcast → Echo Grid |
| in | **4211** | `echo_cmd` from Echo Grid |

## Boards

| Flag | Board |
|------|--------|
| `--standard` | ESP32 DevKit |
| `--cyd` | Cheap Yellow Display |
| `--s3` | ESP32-S3 |

## Flash one CYD

```bash
cd wifi-sensing-system/esp32
git pull
chmod +x flash.sh

./flash.sh --cyd -p /dev/ttyUSB0 -e --monitor
```

Display shows node id, rate, boost, CSI bars.

## Flash additional nodes (unique ids)

```bash
# second CYD
./flash.sh --cyd --node esp32_cyd_02 -p /dev/ttyUSB0 -e

# second plain ESP32
./flash.sh --standard --node esp32_node_02 -p /dev/ttyUSB1 -e

# third
./flash.sh --standard --node esp32_node_03 -p /dev/ttyACM0 -e
```

Each node needs its own `NODE_ID` so Echo Grid tracks them separately.

## First boot

1. Join `ESP32-CSI-<node>` if portal opens  
2. Set home Wi‑Fi  
3. Same LAN as PC running Echo Grid  

## Echo Grid

```bash
python visualization/dashboard.py --csi
```

ESP serial should show `[CMD] boost` / `quiet` when the grid is live.
