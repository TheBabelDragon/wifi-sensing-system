# ESP32 CSI Node - Fully Hardened Production Firmware

This is the **hardened, production-ready** version of the ESP32 CSI sensing node.

## Hardening Features Added

- **Watchdog Timer** — Automatically resets if the device freezes
- **Robust WiFi reconnection** with exponential backoff
- **NVS Preferences** — Persistently stores `node_id` and `target_ip` across reboots
- **Status LED patterns** — Visual feedback for connection and health
- Professional auto-updating web dashboard
- Clean, reliable CSI streaming

## Quick Start

1. Flash `esp32_csi_udp_sender.ino`
2. The device will use the hardcoded WiFi credentials on first boot
3. Open the web dashboard at the printed IP address

The node is now much more reliable for long-term deployment.

## Persistent Configuration

After first boot, `node_id` and `target_ip` are saved in NVS and will survive power cycles.

## Recommended for

- Real mining facility deployments
- Long-running sensor networks
- Production use with aurora-swarm-btc

This version is designed to be reliable and low-maintenance.