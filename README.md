# WiFi CSI Spatial Intelligence System v1.1.0 + Aurora Swarm BTC Integration

**Complete operable implementation** of a spatially-aware, behaviorally-intelligent compute swarm.

This system merges **physical perception** (WiFi CSI) with **high-performance compute** (Bitcoin mining swarm).

## Architecture Overview

```
[Physical World] → WiFi CSI Sensing → Events + Behavior + Agents → Aurora Swarm BTC
     (occupancy, movement, anomalies)          (real-time decisions)           (mining + control bus)
```

## Key Integration Components

- **AuroraAdapter** — Bidirectional Redis bridge to `aurora-swarm-btc`
- **SwarmBridge** — High-level event publishing (`OCCUPANCY_DETECTED`, `ANOMALY_NEAR_RIGS`, thermal context)
- **SwarmEventListener** — Example subscriber the swarm can use to react to sensing data
- **Agent Layer** — Intelligent policies with cooldowns, priority, and swarm commands (`scale_down`, `pause_mining`)
- **Configurable Pipeline** — Environment-driven via `.env`

## Quick Start

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
cp .env.example .env   # optional
python run_full_pipeline.py
```

## Advanced Usage

```bash
# With live Redis connection to aurora-swarm-btc
REDIS_URL=redis://your-swarm-redis:6379/0 python run_full_pipeline.py

# Run the listener (on swarm side or separately)
python bridges/swarm_listener.py

# Quick integrated demo
python examples/integrated_demo.py
```

## Production Notes

- Set `REDIS_URL` to point at your aurora-swarm-btc Redis instance.
- Sensing events appear on channels: `aurora:sensing:events`, `aurora:sensing:alerts`
- The swarm can subscribe and react (scale power, pause mining, alert operators).

## Related Repository

- [aurora-swarm-btc](https://github.com/TheBabelDragon/aurora-swarm-btc) — The entropy-driven Bitcoin mining swarm now augmented with physical awareness.

They yearn for the mines. Now they also *see* who is near them.