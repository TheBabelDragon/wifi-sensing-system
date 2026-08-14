# ESP32 CSI nodes (secured + ESP-NOW + closed loop)

## Security

| Control | Default |
|---------|---------|
| Unique node ID | MAC-based `csi-A1B2C3` / `cyd-A1B2C3` |
| Setup portal | **WPA2** password `echogrid1` |
| Packet auth | Shared secret tag on UDP + ESP-NOW payloads |
| Command auth | `echo_cmd` boost/rate require auth tag |
| ESP-NOW PMK | Derived from shared secret |

**Change before production:**

```ini
; platformio.ini build_flags
-DECHO_SECRET=\"your-long-random-secret\"
-DECHO_PORTAL_PASS=\"your-portal-pass\"
```

Host must use the same secret:

```bash
export ECHO_SECRET='your-long-random-secret'
python visualization/dashboard.py --csi --secret "$ECHO_SECRET"
```

Defaults are in `esp32/include/echo_secret.h` (`echogrid-change-me` / `echogrid1`).

## Boot

1. Unique ID from MAC  
2. Try saved WiFi → else portal AP `CSI-<id>` (password-protected)  
3. CSI + ESP-NOW  
4. UDP CSI with `auth` tag when on LAN  

## Flash

```bash
cd esp32 && git pull
./flash.sh --standard -p /dev/ttyUSB0 -e --monitor
./flash.sh --cyd -p /dev/ttyUSB1 -e --monitor
```

Portal on phone: join `CSI-…`, password **`echogrid1`** (unless you changed it).

## Closed-loop Echo Grid

```bash
python visualization/dashboard.py --csi --secret echogrid-change-me
```

- Receives CSI on **:4210**
- Sends `echo_cmd` field/boost/quiet on **:4211**
- Nodes show field mirror (CYD UI + internal state)
