# WiFi CSI Spatial Intelligence System v1.1.0 + Aurora Swarm BTC Integration

**Complete operable implementation** of a spatially-aware, behaviorally-intelligent compute swarm.

This system merges **physical perception** (WiFi CSI) with **high-performance compute** (Bitcoin mining swarm) and is **ready for easy deployment on ESP32 hardware**.

## Architecture Overview

```
[ESP32 Nodes] → UDP CSI Stream → Python Pipeline → Agents + SwarmBridge → Aurora Swarm BTC
     (easy flash)         (real-time)           (decisions)           (mining control)
```

## ESP32 Support — Easy Flashing

The system includes ready-to-flash firmware for ESP32 devices.

### Quick Start (Arduino IDE — Recommended for beginners)

1. Install [Arduino IDE](https://www.arduino.cc/en/software) + ESP32 board support
2. Open the file: `esp32/esp32_csi_udp_sender.ino`
3. Edit the top section:
   - `ssid` and `password` (your WiFi)
   - `target_ip` (IP address of the machine running the Python pipeline)
   - `node_id` (unique name for this sensor, e.g. `esp32_mining_hall_01`)
4. Upload to your ESP32

The ESP32 will immediately start capturing CSI and streaming JSON data over UDP to the central system.

### Multiple Nodes
Just flash multiple ESP32s with different `node_id` values. The Python pipeline automatically handles them.

See `esp32/README.md` for more details and advanced options (ESP-IDF).

## Key Integration Components

- **ESP32 Firmware** — Easy Arduino flashable CSI capture + UDP sender
- **AuroraAdapter** — Bidirectional Redis bridge to `aurora-swarm-btc`
- **SwarmBridge** + **SwarmEventListener** — Event publishing and subscription
- **Agent Layer** — Intelligent policies with cooldowns and swarm commands
- **Configurable Pipeline** — Environment-driven via `.env`

## Quick Start (Full System)

```bash
git clone https://github.com/TheBabelDragon/wifi-sensing-system.git
cd wifi-sensing-system
cp .env.example .env
python run_full_pipeline.py
```

Flash one or more ESP32s as described above, and the pipeline will start receiving real CSI data.

## Production Notes

- Set `REDIS_URL` to connect to your aurora-swarm-btc Redis instance.
- Multiple ESP32 nodes can feed the same pipeline.
- Events flow into the swarm control bus for context-aware mining decisions.

## Related Repository

- [aurora-swarm-btc](https://github.com/TheBabelDragon/aurora-swarm-btc) — The entropy-driven Bitcoin mining swarm now with physical spatial awareness.

They yearn for the mines. Now they can *see* who is near them.