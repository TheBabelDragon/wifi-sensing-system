# Sensing ↔ Swarm Integration Contract

This document defines the message contracts and resilience model.

## Channels

### Sensing → Swarm
- `aurora:sensing:context` (FULL_CONTEXT_UPDATE)
- `aurora:sensing:events`
- `aurora:sensing:alerts`
- Heartbeat key: `aurora:sensing:heartbeat`

### Swarm → Sensing
- Channel: `aurora:swarm:commands`

See full payload definitions in this document.

## Resilience
Both sides now implement reconnection with exponential backoff and health monitoring.

## Version
1.1 (June 2026) - Added command channel resilience + backoff