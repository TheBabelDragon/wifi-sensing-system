# WiFi CSI Spatial Intelligence System v1.1.0

**Production-ready system** with Docker support.

## Quick Start with Docker (Recommended)

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system

# Run everything (Python system + Redis)
docker compose up --build
```

Then open **http://localhost:8000** for the live central dashboard.

## Without Docker

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
pip install -r requirements.txt
python run_full_pipeline.py
```