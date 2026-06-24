# WiFi CSI Spatial Intelligence System v1.1.0

**Production-oriented spatial intelligence system** with polished ESP32 nodes and robust Python backend.

## Key Improvements (Beyond Proof of Concept)

- Centralized configuration (`config.py`)
- Proper logging throughout the system
- More robust error handling and auto-reconnect logic
- Cleaner, more maintainable module design
- Professional ESP32 web dashboard
- End-to-end integration with aurora-swarm-btc

## Quick Start

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
python run_full_pipeline.py
```

## ESP32 Nodes

Flash `esp32/esp32_csi_udp_sender.ino`. Open the node's IP in a browser for a clean live dashboard.

## Architecture

Edge (ESP32) → UDP → Ingestion → Fusion → Tracking → Behavior → Events → Agent → Swarm Bridge → aurora-swarm-btc

Ready for real deployment.