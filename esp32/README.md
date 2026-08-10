# ESP32 CSI nodes (closed-loop)

| Direction | Port | Role |
|-----------|------|------|
| out | **4210** | CSI JSON broadcast → Echo Grid |
| in | **4211** | `echo_cmd` from Echo Grid |

## Boot policy

**Sensing first, Wi‑Fi second.**

1. Bring up `WIFI_STA` + hardware CSI immediately  
2. Local spectrum / CYD UI runs even with no LAN  
3. Try saved credentials in the background  
4. Config portal is optional (once, timed) — **not** a hard gate before init  
5. UDP to Echo only when associated

Nodes no longer sit dead until they join the host AP.

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

Display shows node id, rate, boost, CSI bars (works offline).

## Flash additional nodes (unique ids)

```bash
./flash.sh --cyd --node esp32_cyd_02 -p /dev/ttyUSB0 -e
./flash.sh --standard --node esp32_node_02 -p /dev/ttyUSB1 -e
./flash.sh --s3 --node esp32_s3_01 -p /dev/ttyACM0 -e
```

## First boot / credentials

If no saved STA config, a timed portal `ESP32-CSI-<node>` may open **after** CSI is already running. Join it when convenient to store home Wi‑Fi. Same LAN as the Echo host is only required for UDP uplink — not for node init.

## Echo Grid

```bash
python visualization/dashboard.py --csi
```

ESP serial should show `[CMD] boost` / `quiet` when the grid is live.
