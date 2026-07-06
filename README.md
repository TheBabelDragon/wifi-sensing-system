# WiFi CSI Spatial Intelligence System v2.0

**Resilient bidirectional integration with aurora-swarm-btc.**

Real hardware ESP32 CSI sensing nodes now fully supported alongside simulation. See `esp32/` directory for flashing instructions and production firmware.

See `INTEGRATION_CONTRACT.md` for the current message formats and resilience model.

## Quick Start (Simulation)

```bash
docker compose up --build
```

Dashboard: http://localhost:8000

## Real Hardware Nodes (New in v2.0)

Flash ESP32 devices with `esp32/esp32_csi_udp_sender.ino` (or via PlatformIO).

1. Edit WiFi credentials + target server IP in the .ino (or platformio.ini)
2. Upload to ESP32
3. Nodes will stream live RSSI + CSI amplitude data over UDP port 4210 to the ingestor
4. Mix real + simulated data seamlessly in the pipeline

See full docs in `esp32/README.md` (includes troubleshooting, real CSI extension guide, deployment tips).

The system sends rich context to the swarm and can receive commands back.