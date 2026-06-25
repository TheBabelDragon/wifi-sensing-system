# Hybrid Node Architecture (WiFi CSI + LoRa/Meshtastic + PoE)

**Practical Meshtastic Integration Focus**

## Current Approach

We use **pre-assembled Meshtastic devices** (W10 and similar) running official firmware for long-range needs.

Integration happens through **Gateway nodes** that bridge Meshtastic data into the central system.

See the `meshtastic_gateway/` folder for examples and guidance.

## Key Points

- Meshtastic nodes send lightweight summarized data only
- Gateway handles parsing and forwarding
- Raw CSI stays on WiFi nodes
- Clean separation of concerns

This is the maintainable way to add long-range capability to the system.