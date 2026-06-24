# Hybrid Node Architecture (LoRa + Meshtastic + PoE)

This document outlines how the WiFi CSI Spatial Intelligence System supports mixed/hybrid node deployments, including LoRa (via Meshtastic) and PoE-based nodes.

## Goals
- Support long-range / off-grid deployments using LoRa
- Allow mixed environments (WiFi CSI nodes + LoRa mesh nodes + PoE nodes)
- Keep the system modular and extensible
- Maintain clear data flow into the central intelligence layer and aurora-swarm-btc

## Node Types

### 1. Standard WiFi CSI Node (Current Primary)
- ESP32 + WiFi
- Runs custom firmware (`esp32_csi_udp_sender.ino`)
- Performs CSI capture + sends JSON over UDP
- Has built-in web dashboard
- Best for: Indoor/covered areas with good WiFi

### 2. LoRa / Meshtastic Node
- ESP32 + LoRa module (e.g. SX1262/SX1276)
- Can run **Meshtastic firmware** for mesh networking
- Can run custom firmware that bridges CSI data over LoRa
- Best for: Long-range, outdoor, or off-grid deployments

### 3. PoE Node
- ESP32 with PoE support (via PoE splitter or PoE HAT)
- More reliable power for always-on nodes
- Can combine with WiFi or Ethernet backhaul
- Best for: Fixed infrastructure positions

## Recommended Hybrid Patterns

### Pattern A: Meshtastic as Long-Range Backhaul
- Some nodes run full Meshtastic firmware
- They forward sensor data or alerts over the LoRa mesh
- A gateway node bridges Meshtastic → MQTT / UDP → central system

### Pattern B: Custom CSI + LoRa Fallback
- Primary backhaul = WiFi UDP
- Fallback / long-range = LoRa (custom or Meshtastic)
- Useful when WiFi is unreliable

### Pattern C: Mixed PoE + Wireless
- Critical nodes use PoE for reliability
- Mobile / temporary nodes use battery + LoRa/WiFi

## Data Flow in Hybrid Deployments

All node types should ultimately feed into the same central pipeline:

```
ESP32 Node (any type) → Transport (WiFi UDP / LoRa / MQTT)
→ CSIIngestor / Bridge
→ Fusion / Tracking / Behavior / Events
→ SwarmBridge → aurora-swarm-btc
```

## Current Status (v1.1.0)

- Standard WiFi CSI nodes: Fully supported
- LoRa / Meshtastic support: In planning + early scaffolding
- PoE nodes: Supported at hardware level (no firmware changes needed)

## Next Steps

See the staged expansion plan in development discussions.

Stage 1 (Current): Modular firmware + architecture documentation
Stage 2: Add LoRa support scaffolding + Meshtastic integration guidance
Stage 3: Full hybrid examples + PoE best practices

## Notes
- Meshtastic is recommended for long-range mesh because it is mature and actively maintained.
- We will avoid trying to run full CSI processing over LoRa (bandwidth constraints). Instead, we will send summaries, events, or alerts over LoRa.