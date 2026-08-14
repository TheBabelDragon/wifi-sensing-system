# ESP32 CSI nodes (secured + ESP-NOW + closed loop)

## Baked-in credentials (download & flash)

| Item | Value |
|------|--------|
| Shared secret | `Eg7$kQ2mN9pR4vX8wL3hJ6cF1bA5yU0zT` |
| Portal password | `Eg7kQ2mN9p` |
| Node ID | MAC-based `csi-A1B2C3` / `cyd-A1B2C3` |

Firmware and `visualization/dashboard.py` already share this secret. No extra config required.

## Flash

```bash
cd esp32 && git pull
./flash.sh --standard -p /dev/ttyUSB0 -e --monitor
./flash.sh --cyd -p /dev/ttyUSB1 -e --monitor
```

Phone: join **`CSI-<id>`**, password **`Eg7kQ2mN9p`**, set house WiFi.

## Echo Grid

```bash
python visualization/dashboard.py --csi
```

## What security is on

- WPA2 portal (not open)
- Packet auth tags on UDP + ESP-NOW
- Command auth on boost/rate/quiet
- ESP-NOW PMK derived from secret
- Unique per-board IDs

Rotate later with `-DECHO_SECRET=...` / `-DECHO_PORTAL_PASS=...` if this repo is public and you need a private swarm key.
