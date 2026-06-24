# WiFi CSI Spatial Intelligence System v1.1.0

**Fully hardened production platform** with professional ESP32 nodes, central intelligence, live web dashboard, and automatic session logging.

## Highlights

- Hardened ESP32 nodes (Watchdog + NVS + beautiful live UI)
- Robust Python stack with centralized config
- **Central Web Dashboard** at http://localhost:8000
- Automatic session logging to `logs/`
- Direct integration with aurora-swarm-btc

## Quick Start

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
pip install -r requirements.txt
python run_full_pipeline.py
```

Open http://localhost:8000 while it's running for the live central view.

Ready for serious deployment.