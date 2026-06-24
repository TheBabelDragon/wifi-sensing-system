# WiFi CSI Spatial Intelligence System v1.1.0

**Fully hardened, production-oriented spatial intelligence platform** with ESP32 edge nodes and central intelligence.

## New: Central Web Dashboard

When running `python run_full_pipeline.py`, a FastAPI dashboard is automatically started at **http://localhost:8000** showing live system state.

## Quick Start

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
pip install -r requirements.txt
python run_full_pipeline.py
```

## Key Features

- Hardened ESP32 nodes with professional live web UI + Watchdog + persistent config
- Robust Python backend with centralized config and logging
- Optional central FastAPI dashboard
- Direct integration with aurora-swarm-btc
- Multi-node capable

Ready for real deployment and monitoring.