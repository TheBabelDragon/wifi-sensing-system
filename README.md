# WiFi CSI Spatial Intelligence System v2.0

Real hardware ESP32 CSI + secured closed-loop Echo Grid.

## Quick start (as shipped)

```bash
# 1) Flash nodes
cd esp32 && ./flash.sh --standard -p /dev/ttyUSB0 -e --monitor
# portal password: Eg7kQ2mN9p

# 2) Host closed-loop viz (secret already matches firmware)
python visualization/dashboard.py --csi
```

## Ports

- **4210** — CSI JSON in
- **4211** — `echo_cmd` closed-loop out

## Security (baked in)

Shared secret + WPA2 portal password are compiled into firmware and defaulted in the dashboard. Pull, flash, run.

See `esp32/README.md`.
