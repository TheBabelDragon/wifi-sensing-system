# WiFi CSI Spatial Intelligence System v1.1.0

**Production-oriented system** with centralized YAML + environment configuration.

## Configuration

Configure via `config.yaml` or environment variables (env vars take precedence).

Example:
```bash
SIMULATION_FRAMES=12 ROOM_NAME=lab python run_full_pipeline.py
```

## Quick Start

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
pip install -r requirements.txt
python run_full_pipeline.py
```

Open http://localhost:8000 for the live dashboard.