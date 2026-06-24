# WiFi CSI Spatial Intelligence System v1.1.0 + Aurora Swarm BTC

**Fully operational, real-world demonstrational spatial intelligence system** ready for ESP32 deployment and integration with your Bitcoin mining swarm.

This is a complete end-to-end cognition stack:

Perception → Understanding → Decision → Action (in aurora-swarm-btc)

## Quick Demo (Recommended)

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
python run_full_pipeline.py
```

You will see a rich, visual demonstration including:
- Voxel field generation with ASCII visualization
- Multi-object tracking with velocity
- Behavior classification
- Event & anomaly detection
- Intelligent agent decisions
- Direct context pushing to aurora-swarm-btc

## ESP32 Hardware Support

Flash `esp32/esp32_csi_udp_sender.ino` using Arduino IDE (see `esp32/README.md`).
The Python pipeline automatically receives live data from real ESP32 nodes via UDP.

## Full Architecture

All layers are fully implemented with fine-detail logic:
- Ingestion (UDP + simulation)
- Calibration + drift detection
- Fusion (Gaussian voxel fields)
- Tracking with association & velocity
- Predictive tracking (Kalman-style)
- Behavior understanding
- Event engine
- Interaction modeling
- Memory & autonomous adaptation
- Federation
- Intelligent Agent with swarm policies
- Dashboard + visualization
- Aurora Swarm Bridge (Redis)

## Integration with aurora-swarm-btc

Events and decisions from the physical space are pushed into the mining swarm’s control bus, enabling context-aware mining operations (power scaling, security alerts, etc.).

## Related Repo

[aurora-swarm-btc](https://github.com/TheBabelDragon/aurora-swarm-btc)

**They yearn for the mines. Now they can see who is near them.**