# ESP32 CSI Nodes

Stream WiFi CSI to a host on **UDP port 4210**.

Compatible consumers:
- `wifi-sensing-system` CSI ingestor
- [Echo Grid Ultrasonic OS](https://github.com/TheBabelDragon/echo-grid-ultrasonic-os) (`python visualization/dashboard.py --csi`)

## Canonical contract

| Field | Value |
|-------|-------|
| Port | **4210** |
| Protocol | UDP JSON |
| Keys | `node`, `rssi`, `csi` (32 floats), `type: wifi_csi` |

## Server IP

Set once via the WiFiManager portal field **"Echo Grid / CSI host IP"**  
(stored in NVS — survives reboot).

1. Flash firmware
2. Join `ESP32-CSI-<node>` AP if needed
3. Enter your PC IP (machine running Echo Grid)
4. Reboot node → packets flow to `IP:4210`

## Flash

```bash
# PlatformIO (recommended)
./flash.sh --standard

# or Arduino IDE: open esp32_csi_udp_sender.ino
```

## Echo Grid quick path

```bash
# on PC
cd echo-grid-ultrasonic-os
python visualization/dashboard.py --csi

# on ESP32: flash this repo, set host IP in portal to the PC
```
