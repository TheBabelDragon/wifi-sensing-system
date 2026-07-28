# ESP32 CSI Nodes

UDP **port 4210** → Echo Grid / wifi-sensing host.

## If you hit Guru Meditation / reboot loop

Use the **simple** Arduino sketch first:

1. Open `esp32_csi_udp_sender.ino` in Arduino IDE
2. Libraries: ArduinoJson, WiFiManager
3. Board: ESP32 Dev Module
4. Port: `/dev/ttyUSB0`
5. **Tools → Erase Flash → All Flash Contents**
6. Upload

Or PlatformIO (fixed boot order):

```bash
git pull
pio run -e esp32-standard -t erase
pio run -e esp32-standard -t upload --upload-port /dev/ttyUSB0
pio device monitor -b 115200 --port /dev/ttyUSB0
```

You should see `Setup complete` and `[UDP] ...:4210` — **not** continuous reboots.

## Portal

Join `ESP32-CSI-*` → `http://192.168.4.1` → set home Wi‑Fi + **Echo Grid host IP**.

## Echo Grid

```bash
python visualization/dashboard.py --csi
```
