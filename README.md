# WiFi CSI Spatial Intelligence System v1.1.0 + Aurora Swarm BTC Integration

**Complete operable implementation** of a spatially-aware, behaviorally-intelligent compute swarm.

This system merges **physical perception** (WiFi CSI) with **high-performance compute** (Bitcoin mining swarm).

## Architecture Overview

```
[Physical World]  →  WiFi CSI Sensing  →  Events + Behavior + Agents  →  Aurora Swarm BTC
     (people, movement, occupancy)          (real-time decisions)           (mining + control bus)
```

## Key Integration Points

- **AuroraAdapter** — Bidirectional bridge to `aurora-swarm-btc` Redis control bus
- **SwarmBridge** — High-level events (`OCCUPANCY_DETECTED`, `ANOMALY_NEAR_RIGS`, thermal context)
- **Agent Layer** — Swarm-aware policies that can issue `scale_down`, `pause_mining`, etc.
- **Dashboard + Memory** — Shared state between physical sensing and mining swarm

## Quick Start (Integrated)

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
python run_full_pipeline.py
```

The pipeline now demonstrates real event publishing into the aurora-swarm-btc control bus (simulated or live via REDIS_URL).

## Production Integration

Set environment variable:
```bash
export REDIS_URL=redis://your-swarm-redis:6379/0
```

Then run the pipeline — sensing events will appear on channels like:
- `aurora:sensing:events`
- `aurora:sensing:alerts`

The swarm dashboard and scheduler can subscribe to these channels for context-aware decisions.

## Related Repo

- [aurora-swarm-btc](https://github.com/TheBabelDragon/aurora-swarm-btc) — The entropy-driven Bitcoin mining swarm this system now feeds.

They yearn for the mines. Now they also *see* who is near them.