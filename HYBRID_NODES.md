# Hybrid Node Architecture (WiFi CSI + LoRa/Meshtastic + PoE)

**Stage 2 in Progress**

## Latest Progress

- Added clear usage instructions in the firmware for switching transports
- Improved web dashboard messaging for Stage 2
- Documented how to enable LoRa or Meshtastic bridge mode

## How to Switch Transports (Current Firmware)

```cpp
bool use_lora = false;               // Set true for LoRa
bool use_meshtastic_bridge = false;  // Set true for Meshtastic bridge
```

Default behavior remains WiFi UDP (fully functional).

## Stage 2 Focus

Stage 2 is about creating solid, well-documented scaffolding so future implementation of LoRa and Meshtastic bridging can be done cleanly without disrupting existing functionality.

---

See earlier sections for full architecture and node types.