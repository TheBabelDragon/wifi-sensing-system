# Hybrid Node Architecture (WiFi CSI + LoRa/Meshtastic + PoE)

**Stage 1 Document** — Foundation and Architecture

This document defines how the system will support hybrid deployments involving different communication backhauls and power options.

## 1. Node Categories

### Type A: Standard WiFi CSI Node (Current Default)
- Hardware: ESP32 (any common dev board)
- Backhaul: WiFi + UDP
- Power: USB or Battery
- Capabilities: Full CSI capture + local web dashboard
- Use case: Indoor or covered areas with reliable WiFi

### Type B: LoRa / Meshtastic Node
- Hardware: ESP32 + LoRa module (SX1262 / SX1276 recommended)
- Backhaul options:
  - Run **Meshtastic** firmware for mesh networking
  - Run custom firmware that sends data over LoRa
- Power: Battery or solar (typical for long-range nodes)
- Use case: Long-range, outdoor, or off-grid deployments

### Type C: PoE Node
- Hardware: ESP32 with PoE capability (splitter or native PoE board)
- Backhaul: WiFi or Ethernet
- Power: PoE (most reliable for fixed nodes)
- Use case: Permanent installations where power reliability matters

## 2. Recommended Hybrid Patterns

### Pattern 1: Meshtastic as Primary Long-Range Mesh
- Many nodes run full Meshtastic firmware
- They form a resilient long-range mesh
- One or more gateway nodes bridge Meshtastic data into the central system (via MQTT, UDP, or serial)

### Pattern 2: WiFi Primary + LoRa Fallback
- Nodes primarily use WiFi UDP
- Fall back to LoRa (custom or Meshtastic) when WiFi is unavailable
- Good for semi-reliable environments

### Pattern 3: Mixed PoE + Wireless Infrastructure
- Critical fixed nodes use PoE
- Mobile or temporary nodes use battery + LoRa/WiFi

## 3. Data Flow Principles

All node types should ultimately deliver data into the same central pipeline:

```
Node (any type) → Transport Layer (WiFi UDP / LoRa / MQTT / Ethernet)
→ Ingestor / Bridge
→ Fusion → Tracking → Behavior → Events
→ SwarmBridge → aurora-swarm-btc
```

**Important:** We do **not** plan to send raw CSI matrices over LoRa (bandwidth is too limited). Instead we will send:
- Summaries
- Tracks / events
- Alerts
- Heartbeats

## 4. Current Status (Stage 1)

- Standard WiFi CSI nodes: Fully implemented and hardened
- Hybrid architecture documentation: Created
- Firmware modularization: Started
- LoRa / Meshtastic integration: Not yet implemented (scaffolding only)
- PoE support: Already possible at hardware level

## 5. Stage 1 Goals (Current Focus)

- Finish modular firmware structure
- Clearly document all hybrid patterns
- Prepare clean extension points for LoRa/Meshtastic
- Avoid breaking existing WiFi CSI functionality

## 6. Future Stages (High Level)

- Stage 2: Add LoRa support scaffolding + Meshtastic bridging guidance
- Stage 3: Full hybrid examples + deployment playbooks

## Notes & Principles

- Prefer mature solutions (Meshtastic) over building everything from scratch for long-range mesh.
- Keep the central intelligence layer (fusion, tracking, behavior, events) transport-agnostic.
- Maintain strong observability and resilience across all node types.

---

**Version:** Stage 1 (June 2026)