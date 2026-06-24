# ESP32 CSI Node - Professional Live Viewing Edition

This is a polished, reliable ESP32 firmware for the WiFi Spatial Intelligence System.

## Highlights

- **Beautiful modern web dashboard** (auto-refreshing, clean dark theme)
- Real-time metrics: RSSI, Channel, Packets/sec, Total packets
- Reliable CSI capture + UDP streaming
- WiFi auto-reconnect
- Excellent Serial logging for debugging
- Easy configuration at the top of the file

## Quick Start

1. Open `esp32_csi_udp_sender.ino` in Arduino IDE
2. Edit the configuration section:
   ```cpp
   const char* ssid = "YourWiFi";
   const char* password = "YourPassword";
   const char* target_ip = "192.168.1.XXX";  // Your pipeline machine
   const char* node_id = "esp32_mining_hall_01";
   ```
3. Upload to ESP32

## Live Dashboard

After flashing, open in your browser:

```
http://<ESP32-IP-ADDRESS>/
```

You will see a clean, professional dashboard that updates automatically every 2 seconds showing real-time CSI activity.

This is perfect for:
- Quick deployment verification
- Live demonstrations
- Monitoring sensor health

## What Gets Sent to the Pipeline

Clean JSON over UDP containing node ID, RSSI, channel, timestamp, CSI length, and current packets-per-second.

The central Python system receives this and runs the full intelligence stack (tracking, behavior, events, swarm decisions).

## Pro Tips

- Use unique `node_id` for each physical location
- Multiple ESP32s work great together
- The web dashboard works even while data is streaming

This firmware is designed to feel "holy shit, it just works" reliable.