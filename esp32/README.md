# ESP32 CSI nodes

The CYD does not find MetaField by magic. Path:

```
CYD  --UDP 4210 broadcast/unicast-->  host csi-bridge / dashboard.py
                                         |
                                         v
                               /tmp/metafield/csi.jsonl
                                         |
                                         v
                                    hello_view
```

If the CYD screen says **LOC**, it is offline ESP-NOW only. No UDP reaches the PC.
Join house WiFi via portal `CSI-cyd-…` / `Eg7kQ2mN9p`.

Host must listen:

```bash
python3 metafield-engine/scripts/csi-bridge.py
# or
python visualization/dashboard.py --csi
```

Do not run both. They both bind 4210.

After flashing `cyd-discover` firmware: serial line `CSIJSON {…}` every packet,
and `host 192.168.x.x` pins unicast. Host announces `metafield_host` on UDP 4211.

```bash
cd esp32 && ./flash.sh --cyd -p /dev/ttyACM0 -e --monitor
```
