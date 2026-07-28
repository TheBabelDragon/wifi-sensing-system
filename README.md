# WiFi CSI Spatial Intelligence System v2.0

Real hardware ESP32 CSI sensing + simulation pipeline.

## CSI UDP contract (canonical)

- **Port: 4210**
- JSON: `{ "node", "rssi", "csi": [32 floats], "type": "wifi_csi" }`

This is the same port Echo Grid listens on:

```bash
python visualization/dashboard.py --csi
```

## Quick Start (Simulation)

```bash
docker compose up --build
```

Dashboard: http://localhost:8000

## Real Hardware Nodes

```bash
cd esp32
./flash.sh --standard
```

On first boot, set **Echo Grid / CSI host IP** in the WiFiManager portal to the PC running the host.

See `esp32/README.md`.
