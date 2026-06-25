# Hybrid Node Architecture (WiFi CSI + LoRa/Meshtastic + PoE)

**Revised Practical Approach (June 2026)**

## Key Principle

Since devices like **Meshtastic W10** come pre-assembled with mature firmware, we will **not** try to make custom CSI ESP32 nodes do LoRa. Instead, we will focus on clean integration of pre-assembled Meshtastic devices into the system.

## Recommended Architecture

### Node Types

- **WiFi CSI Nodes**: Run custom firmware. Handle full CSI capture. Best for areas with good WiFi.
- **Meshtastic Nodes** (W10 or similar): Run official Meshtastic firmware. Used for long-range / off-grid coverage. Send summarized data only.
- **Gateway Nodes**: Bridge Meshtastic data into the central system (via MQTT, UDP, or serial).
- **PoE Nodes**: Fixed infrastructure nodes with reliable power (can be WiFi or Ethernet).

### Data Strategy

- **WiFi CSI nodes**: Send rich context (tracks, events, full summaries)
- **Meshtastic nodes**: Send lightweight data only:
  - Events / Alerts
  - Occupancy summaries
  - Heartbeats / status
  - Simple track information (if bandwidth allows)

Raw CSI matrices are **never** sent over LoRa.

## Meshtastic Integration Pattern

1. Deploy pre-assembled Meshtastic devices (W10, T-Beam, etc.) running official firmware.
2. Use one or more **Gateway nodes** that:
   - Connect to the Meshtastic mesh (usually via MQTT)
   - Parse incoming messages
   - Forward relevant data to the central system via UDP or MQTT
3. The central system ingests this data the same way it ingests from WiFi nodes.

This approach is clean, maintainable, and leverages mature hardware/firmware.

## Current Status

- WiFi CSI nodes: Fully functional
- Meshtastic integration: Focused on gateway/bridge logic (in progress)
- LoRa in custom CSI firmware: De-prioritized (use pre-assembled Meshtastic devices instead)

## Next Steps

- Create example Meshtastic Gateway structure
- Define message formats for Meshtastic nodes
- Provide simple bridge example (Python)

---

This is the practical path forward.