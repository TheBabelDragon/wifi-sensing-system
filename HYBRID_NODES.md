# Hybrid Node Architecture (WiFi CSI + LoRa/Meshtastic + PoE)

**Stage 2 in Progress**

## Stage 2 Progress

- Improved LoRa scaffolding with better comments and example pinouts (SX1262/SX1276)
- Clearer transport selection logic in firmware
- Added recommended Meshtastic bridging strategy

## Recommended Meshtastic Bridging Strategy

Best practice:

1. Run full Meshtastic firmware on long-range / off-grid nodes
2. Deploy gateway node(s) that:
   - Connect to the Meshtastic mesh (MQTT or serial)
   - Forward summarized intelligence (tracks, events, alerts) to the central system
3. Keep raw CSI processing on WiFi-capable nodes only

This leverages Meshtastic's mature mesh capabilities while keeping the core intelligence layer clean and transport-agnostic.

## Current Firmware State

The ESP32 firmware now includes:
- Placeholder functions: `send_via_lora()` and `send_via_meshtastic()`
- Transport selection inside `send_payload()`
- Clear comments for common LoRa modules

Fully backward compatible with standard WiFi UDP operation.

---

See full node types and hybrid patterns in the sections above.