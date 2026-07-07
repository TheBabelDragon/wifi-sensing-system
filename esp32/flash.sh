#!/usr/bin/env bash
#
# flash.sh - Smart unified flasher for ESP32 CSI nodes
# Supports standard ESP32 and Cheap Yellow Display (CYD)

set -euo pipefail

SKETCH="esp32_csi_unified.ino"
DEFAULT_ENV="esp32-standard"

ENV="${ENV:-$DEFAULT_ENV}"
PORT=""
DO_ERASE=false
AUTO_MONITOR=false

show_help() {
  cat <<EOF
ESP32 CSI Unified Flasher

Usage: $0 [options]

Options:
  --cyd              Flash for Cheap Yellow Display (ESP32-2432S028)
  --standard         Flash for normal ESP32 boards (default)
  -p, --port PORT    Specify serial port
  -e, --erase        Full chip erase before flashing
  -m, --monitor      Start serial monitor after upload
  -h, --help         Show help

Examples:
  $0 --cyd
  $0 --standard -p /dev/ttyUSB0 --monitor
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

print_header() { echo -e "\n\033[1;36m=== $1 ===\033[0m"; }
print_success() { echo -e "\033[1;32m✓ $1\033[0m"; }

print_header "ESP32 CSI Unified Flasher"

echo "Sketch     : $SKETCH"
echo "Environment: $ENV"
echo "Port       : $PORT"
echo

if ! command -v pio >/dev/null 2>&1; then
  echo "Error: PlatformIO CLI not found"
  exit 1
fi

if [[ -z "$PORT" ]]; then
  PORT="/dev/ttyUSB0"
fi

if $DO_ERASE; then
  echo "Performing full chip erase..."
  pio run --target erase --environment "$ENV" --upload-port "$PORT" || true
fi

print_header "Compiling and Uploading"

echo "Running: pio run --target upload --environment $ENV --upload-port $PORT"

if pio run --target upload --environment "$ENV" --upload-port "$PORT"; then
  print_success "Upload successful!"
else
  echo "Upload failed. Try with --erase if you changed partition scheme."
  exit 1
fi

if $AUTO_MONITOR; then
  pio device monitor --environment "$ENV" --port "$PORT"
fi

print_success "Done."
