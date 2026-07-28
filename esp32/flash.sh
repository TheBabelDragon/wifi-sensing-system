#!/usr/bin/env bash
# flash.sh — standard / CYD / multi-node CSI closed-loop
set -euo pipefail

ENV="esp32-standard"
PORT="/dev/ttyUSB0"
NODE=""
DO_ERASE=false
AUTO_MONITOR=false

show_help() {
  cat <<EOF
ESP32 CSI closed-loop flasher (CSI:4210 CMD:4211)

Usage:
  $0 --standard -p /dev/ttyUSB0
  $0 --cyd -p /dev/ttyUSB0 --monitor
  $0 --cyd --node esp32_cyd_02 -p /dev/ttyACM0
  $0 --standard --node esp32_node_02 -p /dev/ttyUSB1 -e

Options:
  --standard     plain ESP32 DevKit
  --cyd          Cheap Yellow Display
  --s3           ESP32-S3
  --node NAME    unique node id (default from env)
  -p PORT        serial port
  -e             erase flash first
  -m|--monitor   open serial after flash
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cyd)        ENV="esp32-cyd"; shift ;;
    --standard)   ENV="esp32-standard"; shift ;;
    --s3)         ENV="esp32-s3"; shift ;;
    --node)       NODE="$2"; shift 2 ;;
    -p|--port)    PORT="$2"; shift 2 ;;
    -e|--erase)   DO_ERASE=true; shift ;;
    -m|--monitor) AUTO_MONITOR=true; shift ;;
    -h|--help)    show_help; exit 0 ;;
    *)            PORT="$1"; shift ;;
  esac
done

if ! command -v pio >/dev/null 2>&1; then
  echo "PlatformIO not found (pipx install platformio)"
  exit 1
fi

EXTRA_FLAGS=()
if [[ -n "$NODE" ]]; then
  # escape quotes for -DNODE_ID_STR="name"
  EXTRA_FLAGS+=(--build-flag "-DNODE_ID_STR=\"${NODE}\"")
  echo "Node id: $NODE"
fi

echo "Env=$ENV  Port=$PORT  CSI:4210  CMD:4211"

if $DO_ERASE; then
  pio run -e "$ENV" -t erase --upload-port "$PORT" || true
fi

pio run -e "$ENV" -t upload --upload-port "$PORT" "${EXTRA_FLAGS[@]+"${EXTRA_FLAGS[@]}"}"

if $AUTO_MONITOR; then
  pio device monitor -e "$ENV" --port "$PORT" -b 115200
fi

echo "Done. Join WiFi portal if needed (ESP32-CSI-<node>), then run Echo Grid --csi"
