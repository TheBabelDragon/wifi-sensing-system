# ESP32 CSI Firmware for WiFi Sensing System

This folder contains ready-to-flash firmware for ESP32 devices to act as CSI sensing nodes.

## Quick Flash (Arduino IDE - Easiest)

1. Install Arduino IDE + ESP32 board support (https://docs.espressif.com/projects/arduino-esp32/)
2. Open `esp32_csi_udp_sender.ino`
3. Set your WiFi credentials and target host IP (the machine running the Python pipeline)
4. Upload to ESP32

## Features
- Captures CSI data from WiFi packets
- Sends structured JSON over UDP to the central pipeline
- Low power / configurable interval
- Multiple nodes supported (each with unique node_id)

## Hardware
- Any ESP32 (DevKit, NodeMCU, etc.)
- 2.4GHz WiFi (required for CSI)

After flashing, the ESP32 will start streaming CSI data. The Python `CSIIngestor` can receive it via UDP.

See main README for full system integration with aurora-swarm-btc.