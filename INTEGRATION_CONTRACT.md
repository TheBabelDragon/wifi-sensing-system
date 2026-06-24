# Sensing ↔ Swarm Integration Contract

This document defines the current message contracts and resilience expectations between the WiFi CSI Spatial Intelligence System and aurora-swarm-btc.

## Goals
- Make the integration explicit and concrete
- Define clear message formats
- Establish resilience expectations
- Reduce implicit assumptions

## Channels (Redis Pub/Sub)

### Sensing → Swarm

**Channel:** `aurora:sensing:context`
**Type:** Pub/Sub + State key `aurora:sensing:latest_context`

**Payload (FULL_CONTEXT_UPDATE):**
```json
{
  "type": "FULL_CONTEXT_UPDATE",
  "timestamp": <unix timestamp>,
  "tracks": [...],
  "events": [...],
  "behaviors": [...],
  "memory_summary": {...},
  "source": "wifi-csi-sensing"
}
```

**Channel:** `aurora:sensing:events`
**Payload example:**
```json
{
  "type": "OCCUPANCY_DETECTED",
  "room": "mining_hall",
  "count": <int>,
  "behavior": <string|null>,
  "source": "wifi-csi-sensing"
}
```

**Channel:** `aurora:sensing:alerts`

**Heartbeat:**
- Key: `aurora:sensing:heartbeat`
- Updated regularly by sensing system
- Used by swarm to detect stale data

## Swarm → Sensing

**Channel:** `aurora:swarm:commands`

**Expected Command Format:**
```json
{
  "action" or "type": "scale_down" | "security_mode" | "scale_up",
  "factor": <float>,           // for scale commands
  "duration_minutes": <int>,   // for security_mode
  "reason": <string>,          // optional
  "source": "sensing"
}
```

## Resilience Expectations

- Both sides should implement reconnection logic
- Heartbeat staleness detection on swarm side
- Graceful degradation when integration is unhealthy
- Clear logging of integration health status

## Current Status

This contract is implemented and functional as of v1.1.0, with the following caveats:
- Commands are currently unauthenticated
- Suitable for trusted/internal environments only
- Future versions will add authentication and stricter validation

## Version
Contract Version: 1.0 (June 2026)