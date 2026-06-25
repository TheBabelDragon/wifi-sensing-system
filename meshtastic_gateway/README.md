# Meshtastic Gateway

This folder contains examples and guidance for bridging data from pre-assembled Meshtastic devices (such as W10) into the central WiFi CSI system.

## Recommended Approach

- Use official Meshtastic firmware on long-range nodes
- Deploy one or more gateway nodes that receive from the Meshtastic mesh
- Forward only summarized/useful data to the central system

## Example Files

- `example_bridge.py`: Basic structure for a gateway bridge

## Data Recommendations

Send from Meshtastic nodes:
- Events and alerts
- Occupancy / presence summaries
- Node status / heartbeats

Do not send raw CSI data over Meshtastic.