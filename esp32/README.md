# ESP32 CSI nodes (closed-loop + ESP-NOW)

| Direction | Port / Path | Role |
|-----------|-------------|------|
| out | **4210** (UDP) | Full CSI JSON → Echo Grid (when WiFi joined) |
| out | **ESP-NOW** | Compact CSI broadcast (always, all nodes) |
| in | **4211** (UDP) | `echo_cmd` from Echo Grid |

## Boot policy

**Sensing first. ESP-NOW always on. WiFi optional.**

1. `WIFI_STA` + hardware CSI start immediately  
2. ESP-NOW initialized on every node (broadcast CSI)  
3. If not associated → lock **channel 6** (stable promiscuous CSI + ESP-NOW)  
4. If NVS has saved STA creds → background join home AP  
5. If virgin (no creds) → stay local; **no auto portal**  
6. Portal only when you ask: serial `portal`  
7. Full UDP JSON only while associated; ESP-NOW works with or without WiFi  
8. Any node that *is* on WiFi acts as a **soft gateway** (forwards peer ESP-NOW CSI → UDP)

No host failsafe. CSI and ESP-NOW keep running even if the Echo Grid host is down.

## Serial commands

```
portal   # open WiFiManager AP once, save home Wi‑Fi
status   # wifi/creds/csi/espnow/channel/packet counts
```

## Boards

| Flag / Env          | Board                  |
|---------------------|------------------------|
| `--standard` / `esp32-standard` | ESP32 DevKit     |
| `--cyd` / `esp32-cyd`           | Cheap Yellow Display |
| `--s3` / `esp32-s3`             | ESP32-S3         |

## Flash

This is a **PlatformIO** project (`platformio.ini` + `src/main.cpp`).  
`flash.sh` is a thin convenience wrapper around `pio run`.

### Easy path (`flash.sh`)

```bash
cd wifi-sensing-system/esp32
git pull
chmod +x flash.sh

# Standard DevKit
./flash.sh --standard -p /dev/ttyUSB0
./flash.sh --standard --node esp32_node_02 -p /dev/ttyUSB0 -e --monitor

# Cheap Yellow Display
./flash.sh --cyd --node esp32_cyd_02 -p /dev/ttyUSB1 -e

# ESP32-S3
./flash.sh --s3 --node esp32_s3_01 -p /dev/ttyACM0 -e
```

### Pure PlatformIO (`pio run`)

```bash
cd wifi-sensing-system/esp32

# Optional erase
pio run -e esp32-standard -t erase --upload-port /dev/ttyUSB0

# Upload (uses default NODE_ID from platformio.ini)
pio run -e esp32-standard -t upload --upload-port /dev/ttyUSB0

# Upload with custom node ID
pio run -e esp32-standard -t upload --upload-port /dev/ttyUSB0 \
  --build-flag '-DNODE_ID_STR="esp32_node_02"'

# Serial monitor
pio device monitor -e esp32-standard --port /dev/ttyUSB0 -b 115200
```

Same pattern works for the other environments (`esp32-cyd`, `esp32-s3`).

### Linux / Arch notes

```bash
# one-time serial permissions
sudo usermod -aG uucp,dialout $USER   # then log out / in

# optional but helpful udev rules for common ESP32 USB chips
sudo tee /etc/udev/rules.d/99-esp32.rules <<'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## First credentials (virgin node)

1. Flash and boot — CSI + ESP-NOW already live (channel 6)
2. Open serial monitor and type `portal`
3. Join the temporary AP `ESP32-CSI-<node>` and set home Wi-Fi
4. After join, the node can bridge peer ESP-NOW traffic to the host via UDP

## How the hybrid works

- **Every node** broadcasts compact CSI over ESP-NOW (no WiFi required).
- **Any node that has joined WiFi** also sends full JSON on UDP :4210 and forwards ESP-NOW packets it hears from peers.
- So one provisioned node is enough to get the whole swarm into Echo Grid.
- Offline / virgin nodes still contribute via ESP-NOW to any nearby bridge.

## Echo Grid

```bash
python visualization/dashboard.py --csi
```
