# ESP32 CSI Firmware - Advanced Live Viewing Edition

This firmware turns any ESP32 into a powerful WiFi CSI sensing node with **built-in live viewing**.

## Key Features

- Real-time CSI capture
- UDP streaming to central Python pipeline
- **Built-in Web Server** for live viewing (auto-refreshing dashboard)
- Rich JSON payload (RSSI, channel, packet count)
- Serial debug output
- Easy configuration

## Quick Flash (Arduino IDE)

1. Install Arduino IDE + ESP32 board support
2. Open `esp32_csi_udp_sender.ino`
3. Edit the configuration section at the top:
   - WiFi credentials
   - `target_ip` (IP of your Python pipeline machine)
   - `node_id` (unique name for this sensor)
4. Upload

## Live Viewing

After flashing, open a browser and go to:

```
http://<ESP32-IP-ADDRESS>/
```

You will see a live-updating page showing:
- Current RSSI and channel
- Packet count
- Connection status
- Pipeline target

The page auto-refreshes every 2 seconds.

This is extremely useful for:
- Deployment debugging
- Verifying coverage
- Live monitoring during testing
- Demonstrations

## Data Sent to Pipeline

The ESP32 sends structured JSON over UDP containing:
- node_id
- rssi
- channel
- timestamp
- csi_len
- packet_count

The central `CSIIngestor` receives this data and feeds the full intelligence stack.

## Tips

- Place multiple ESP32s for better spatial coverage
- Use unique `node_id` for each
- The web interface works even while streaming data

For production, you can extend this firmware with:
- Full CSI matrix transmission
- MQTT instead of UDP
- OLED display support
- Deep sleep modes