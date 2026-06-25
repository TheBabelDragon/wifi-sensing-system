# Hybrid Node Architecture (WiFi CSI + LoRa/Meshtastic + PoE)

**Stage 2 in Progress**

## Current Status

- Stage 1 (Modular foundation): Complete
- Stage 2 (LoRa scaffolding + Meshtastic guidance): In progress

## Stage 2 Progress

- Added basic LoRa transport scaffolding in ESP32 firmware
- Added placeholders for `send_via_lora()` and `send_via_meshtastic()`
- Transport selection logic started in `send_payload()`
- Still fully backward compatible with WiFi UDP

## Meshtastic Bridging Guidance (Draft)

Recommended approach:
- Run Meshtastic on dedicated long-range nodes
- Use a gateway node that bridges Meshtastic → MQTT or UDP
- The gateway can be a regular ESP32 or a small server
- Send summarized data (tracks, events, alerts) rather than raw CSI

## Next in Stage 2

- Improve LoRa pin configuration examples
- Add more detailed Meshtastic bridging examples
- Finalize transport selection logic

---

See full architecture in earlier sections of this document.