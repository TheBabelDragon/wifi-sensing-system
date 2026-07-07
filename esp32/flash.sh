#!/usr/bin/env bash
#
# flash.sh - Unified ESP32 CSI flasher (src/main.cpp structure)

set -euo pipefail

ENV="esp32-standard"
PORT=""
DO_ERASE=false
AUTO_MONITOR=false

show_help() {
  cat <<EOF
ESP32 CSI Flasher (src/main.cpp)

Usage:
  $0 --standard          # Normal ESP32 boards
  $0 --cyd               # Cheap Yellow Display
  $0 --cyd -p /dev/ttyUSB0 --monitor
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cyd)       ENV="esp32-cyd"; shift ;;
    --standard)  ENV="esp32-standard"; shift ;;
    -p|--port)   PORT="$2"; shift 2 ;;
    -e|--erase)  DO_ERASE=true; shift ;;
    -m|--monitor) AUTO_MONITOR=true; shift ;;
    -h|--help)   show_help; exit 0 ;;
    *)           if [[ -z "$PORT" ]]; then PORT="$1"; fi; shift ;;
  esac
done

if [[ -z "$PORT" ]]; then
  PORT="/dev/ttyUSB0"
fi

if ! command -v pio >/dev/null 2>&1; then
  echo "PlatformIO not found"
  exit 1
fi

if $DO_ERASE; then
  pio run --target erase --environment "$ENV" --upload-port "$PORT" || true
fi

echo "Uploading with environment: $ENV"
pio run --target upload --environment "$ENV" --upload-port "$PORT"

if $AUTO_MONITOR; then
  pio device monitor --environment "$ENV" --port "$PORT"
fi

echo "Done."
