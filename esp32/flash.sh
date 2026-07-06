#!/usr/bin/env bash
#
# flash.sh - Smart unified flasher for ESP32 CSI nodes
# Supports both standard ESP32 and Cheap Yellow Display (CYD)

set -euo pipefail

SKETCH="esp32_csi_unified.ino"
DEFAULT_ENV="esp32-standard"

ENV="${ENV:-$DEFAULT_ENV}"
PORT=""
DO_ERASE=false
AUTO_MONITOR=false
USE_CYD=false

show_help() {
  cat <<EOF
ESP32 CSI Unified Flasher

Usage: $0 [options]

Options:
  --cyd              Use Cheap Yellow Display (ESP32-2432S028)
  --standard         Use normal ESP32 board (default)
  -p, --port PORT    Serial port
  -e, --erase        Full chip erase first
  -m, --monitor      Start serial monitor after upload
  -h, --help         Show this help

Examples:
  $0 --cyd
  $0 --standard -p /dev/ttyUSB0 --monitor
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cyd)       USE_CYD=true; ENV="esp32-cyd"; shift ;;
    --standard)  USE_CYD=false; ENV="esp32-standard"; shift ;;
    -p|--port)   PORT="$2"; shift 2 ;;
    -e|--erase)  DO_ERASE=true; shift ;;
    -m|--monitor) AUTO_MONITOR=true; shift ;;
    -h|--help)   show_help; exit 0 ;;
    *)           if [[ -z "$PORT" ]]; then PORT="$1"; fi; shift ;;
  esac
done

print_header() { echo -e "\n\033[1;36m=== $1 ===\033[0m"; }
print_success() { echo -e "\033[1;32m✓ $1\033[0m"; }

print_header "ESP32 CSI Unified Flasher v4"

echo "Sketch : $SKETCH"
echo "Environment : $ENV"
echo "Port   : $PORT"
if $USE_CYD; then
  echo "Target : Cheap Yellow Display (2.8\")"
else
  echo "Target : Standard ESP32"
fi
echo

if ! command -v pio >/dev/null; then
  echo "Error: PlatformIO not found in PATH"
  exit 1
fi

if [[ -z "$PORT" ]]; then
  # Simple fallback - user can specify
  PORT="/dev/ttyUSB0"
fi

if $DO_ERASE; then
  echo "Erasing flash..."
  pio run --target erase --environment "$ENV" --upload-port "$PORT" || true
fi

print_header "Building and Uploading"

echo "pio run --target upload --environment $ENV --upload-port $PORT"

if pio run --target upload --environment "$ENV" --upload-port "$PORT"; then
  print_success "Upload successful!"
else
  echo "Upload failed. Check connections and try --erase if needed."
  exit 1
fi

if $AUTO_MONITOR; then
  pio device monitor --environment "$ENV" --port "$PORT"
fi

print_success "Done."
