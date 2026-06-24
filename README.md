# WiFi CSI Spatial Intelligence System v1.1.0

**Now with early bidirectional integration to aurora-swarm-btc.**

## Important Notice

The command channel (swarm → sensing) is in an early stage:
- Currently unauthenticated
- Intended for trusted/internal environments only
- Future versions will include proper security controls

When running the demo, the system now listens for commands from the swarm side.

## Quick Start

```bash
docker compose up --build
```

Dashboard: http://localhost:8000
Metrics: http://localhost:8001/metrics