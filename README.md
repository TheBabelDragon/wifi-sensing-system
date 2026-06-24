# WiFi CSI Spatial Intelligence System v1.1.0 + Aurora Swarm BTC

**Fully operational spatial intelligence system** with powerful ESP32 nodes featuring **built-in live web viewing**.

## ESP32 Live Viewing (New)

Each ESP32 node now runs a built-in web server.

After flashing, simply open:

```
http://<ESP32-IP-ADDRESS>/
```

You get a beautiful, auto-refreshing live dashboard showing real-time CSI data, RSSI, channel, and packet statistics — perfect for deployment, debugging, and live demonstrations.

## Quick Start

```bash
python run_full_pipeline.py
```

Flash ESP32 nodes using `esp32/esp32_csi_udp_sender.ino` (see `esp32/README.md`).

The system now provides end-to-end WiFi tracking with live visibility at both the edge (ESP32 web UI) and the center (Python pipeline + visualization).

## Full Capabilities

- Real CSI capture on ESP32
- Live web dashboard per node
- UDP streaming to central intelligence stack
- Voxel-based spatial reconstruction
- Multi-object tracking with velocity
- Behavior understanding & event detection
- Intelligent agent decisions
- Direct integration with aurora-swarm-btc mining swarm

Ready for real-world deployment.