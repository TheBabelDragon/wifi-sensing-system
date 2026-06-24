# WiFi CSI Spatial Intelligence System v1.1.0

**Fully hardened, production-ready spatial intelligence platform.**

## Current Capabilities

- Hardened ESP32 nodes with Watchdog, NVS config, and beautiful live web UI
- Robust Python intelligence stack
- **Live Central Dashboard** at http://localhost:8000 (with voxel visualization)
- Automatic session logging
- Periodic health/context reporting to aurora-swarm-btc
- Multi-node capable

## Quick Start

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
pip install -r requirements.txt
python run_full_pipeline.py
```

Open http://localhost:8000 while running to see the live central view.

Ready for real deployment.