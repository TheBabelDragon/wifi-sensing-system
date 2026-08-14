# WiFi CSI Spatial Intelligence System v2.0

Real hardware ESP32 CSI sensing + closed-loop Echo Grid.

## CSI UDP contract

- **Port 4210** — `wifi_csi` JSON (`node`, `rssi`, `csi[32]`, `auth`, …)
- **Port 4211** — `echo_cmd` closed-loop feedback (`field`, `boost`, `quiet`, …)

## Echo Grid (closed loop)

```bash
export ECHO_SECRET='echogrid-change-me'   # must match firmware
python visualization/dashboard.py --csi --secret "$ECHO_SECRET"
```

## Hardware nodes

```bash
cd esp32
./flash.sh --standard -p /dev/ttyUSB0 -e --monitor
./flash.sh --cyd -p /dev/ttyUSB1 -e --monitor
```

- Unique MAC-based names (`csi-A1B2C3`, `cyd-A1B2C3`)
- Portal AP is **WPA2**-protected (default pass `echogrid1`)
- Packet + command auth via shared secret  

See `esp32/README.md`.

## Simulation

```bash
docker compose up --build
```

Dashboard: http://localhost:8000
