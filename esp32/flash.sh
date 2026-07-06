#!/usr/bin/env bash
#
# flash.sh - Headless ESP32 CSI UDP Sender flashing helper
# Makes the full Arduino CLI + esptool.py "throughput" process trivial from the console.
#
# Usage:
#   ./flash.sh [PORT] [FQBN]
# Examples:
#   ./flash.sh /dev/ttyUSB0
#   ./flash.sh /dev/ttyACM0 esp32:esp32:esp32s3
#   ./flash.sh COM3 esp32:esp32:esp32dev
#
# Or set environment variables:
#   PORT=/dev/ttyUSB0 FQBN=esp32:esp32:esp32dev ./flash.sh
#
# After flashing it can optionally start a serial monitor.

set -euo pipefail

# ==================== CONFIGURATION (edit these defaults) ====================

SKETCH="esp32_csi_udp_sender.ino"
DEFAULT_FQBN="esp32:esp32:esp32dev"          # Change to esp32:esp32:esp32s3, esp32:esp32:esp32c3, etc.
DEFAULT_PORT="/dev/ttyUSB0"                 # Common on Linux; use COMx on Windows
BUILD_DIR="./build"
FLASH_BAUD=921600
MONITOR_BAUD=115200

# You can also override via environment variables before running:
#   FQBN=... PORT=... ./flash.sh

FQBN="${FQBN:-$DEFAULT_FQBN}"
PORT="${PORT:-$DEFAULT_PORT}"

# Allow first positional argument to override PORT
if [[ $# -ge 1 && $1 != --* ]]; then
  PORT="$1"
fi

# Allow second positional argument to override FQBN
if [[ $# -ge 2 && $2 != --* ]]; then
  FQBN="$2"
fi

# ==================== HELPER FUNCTIONS ====================

print_header() {
  echo -e "\n\033[1;36m=== $1 ===\033[0m"
}

print_success() {
  echo -e "\033[1;32m✓ $1\033[0m"
}

print_error() {
  echo -e "\033[1;31m✗ $1\033[0m" >&2
}

command_exists() {
  command -v "$1" >/dev/null 2>&1
}

# ==================== PRE-FLIGHT CHECKS ====================

print_header "ESP32 CSI UDP Sender - Headless Flasher"

echo "Sketch : $SKETCH"
echo "FQBN   : $FQBN"
echo "Port   : $PORT"
echo "Build  : $BUILD_DIR"

echo

if ! command_exists arduino-cli; then
  print_error "arduino-cli not found in PATH"
  echo "  Install it with:"
  echo "    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
  exit 1
fi

if ! command_exists esptool.py && ! command_exists esptool; then
  print_error "esptool.py not found in PATH"
  echo "  Install it with: pip install --upgrade esptool"
  exit 1
fi

ESPTool="esptool.py"
if command_exists esptool; then
  ESPTool="esptool"
fi

if [[ ! -f "$SKETCH" ]]; then
  print_error "Sketch not found: $SKETCH"
  echo "  Make sure you are running this script from the esp32/ directory."
  exit 1
fi

# ==================== COMPILE ====================

print_header "Compiling sketch (headless)"

echo "Running: arduino-cli compile --fqbn $FQBN --output-dir $BUILD_DIR $SKETCH"

if arduino-cli compile --fqbn "$FQBN" --output-dir "$BUILD_DIR" "$SKETCH"; then
  print_success "Compilation successful"
else
  print_error "Compilation failed"
  exit 1
fi

echo
ls -lh "$BUILD_DIR"/*.bin 2>/dev/null || true

# ==================== FLASH ====================

print_header "Flashing with esptool.py"

echo "Target : $PORT @ ${FLASH_BAUD} baud"
echo "Offsets: 0x1000 (bootloader) + 0x8000 (partitions) + 0x10000 (app)"

echo

echo "Running: $ESPTool --chip esp32 --port $PORT --baud $FLASH_BAUD write_flash -z ..."

if $ESPTool --chip esp32 \
     --port "$PORT" \
     --baud "$FLASH_BAUD" \
     write_flash -z \
       0x1000  "$BUILD_DIR/${SKETCH%.ino}.ino.bootloader.bin" \
       0x8000  "$BUILD_DIR/${SKETCH%.ino}.ino.partitions.bin" \
       0x10000 "$BUILD_DIR/${SKETCH%.ino}.ino.bin"; then

  print_success "Flashing completed successfully!"
else
  print_error "Flashing failed"
  echo "  Common fixes:"
  echo "    - Put board in download mode (hold BOOT while pressing RESET)"
  echo "    - Try a different port or lower baud rate"
  echo "    - Check cable / permissions (Linux: sudo usermod -a -G dialout \$USER)"
  exit 1
fi

# ==================== OPTIONAL MONITOR ====================

echo
read -r -p "Start serial monitor now? [y/N] " response
if [[ "$response" =~ ^[Yy]$ ]]; then
  print_header "Starting serial monitor (${MONITOR_BAUD} baud)"
  if command_exists arduino-cli; then
    arduino-cli monitor --port "$PORT" --config baudrate=$MONITOR_BAUD
  elif command_exists screen; then
    screen "$PORT" $MONITOR_BAUD
  else
    echo "Install 'screen' or use: arduino-cli monitor ..."
  fi
fi

echo
print_success "Done. Your ESP32 CSI node should now be sending UDP packets to port 4210."
