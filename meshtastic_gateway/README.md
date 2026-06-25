# Meshtastic Gateway

Examples and guidance for integrating pre-assembled Meshtastic devices (W10, etc.) into the system.

## Recommended Flow

1. Meshtastic nodes send lightweight summarized data
2. Gateway receives via MQTT (or serial)
3. Gateway forwards useful data to the central CSI system

## Example Files

- `example_bridge.py`: Structured gateway with MQTT support and message filtering

## Suggested Message Types from Meshtastic

- `occupancy`
- `event`
- `alert`
- `heartbeat`

See `example_bridge.py` for the current structure.