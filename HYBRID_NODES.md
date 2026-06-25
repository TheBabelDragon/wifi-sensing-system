# Hybrid Node Architecture (WiFi CSI + LoRa/Meshtastic + PoE)

**Stage 2 in Progress**

## Recent Progress

- Improved LoRa scaffolding with clearer pinout recommendations (SX1262/SX1276)
- Better structured transport selection
- Strengthened Meshtastic bridging strategy documentation

## Current Firmware Capabilities (Stage 2)

The ESP32 firmware now supports:
- WiFi UDP (default, fully functional)
- Placeholder LoRa transport
- Placeholder Meshtastic bridging
- Easy switching via flags (`use_lora`, `use_meshtastic_bridge`)

## Recommended Architecture

- Use Meshtastic for long-range mesh networking
- Use gateway nodes to bridge Meshtastic data into the central system
- Keep raw CSI processing on WiFi nodes
- Send only summarized data over LoRa (tracks, events, alerts)

Stage 2 is focused on solid scaffolding and clear guidance rather than full implementation.

---

See earlier sections for full node types and hybrid patterns.