#!/usr/bin/env bash
#
# flash.sh - Smart headless ESP32 CSI UDP Sender flasher
# Auto-detects serial port, wraps arduino-cli + esptool.py with nice UX.
#
# Usage examples:
#   ./flash.sh
#   ./flash.sh /dev/ttyUSB0
#   ./flash.sh -p /dev/ttyACM0 -b esp32:esp32:esp32s3
#   ./flash.sh --erase --monitor
#   PORT=/dev/ttyUSB0 ./flash.sh
#

set -euo pipefail

# ==================== CONFIGURATION ====================

SKETCH="esp32_csi_udp_sender.ino"
DEFAULT_FQBN="esp32:esp32:esp32dev"
DEFAULT_PORT=""
BUILD_DIR="./build"
FLASH_BAUD=921600
MONITOR_BAUD=115200

FQBN="${FQBN:-$DEFAULT_FQBN}"
PORT="${PORT:-$DEFAULT_PORT}"
DO_ERASE=false
AUTO_MONITOR=false

# ==================== ARGUMENT PARSING ====================

show_help() {
  cat <<EOF
ESP32 CSI UDP Sender - Headless Flashing Helper

Usage: $0 [options] [PORT] [FQBN]

Options:
  -p, --port PORT     Serial port (e.g. /dev/ttyUSB0, COM3)
  -b, --board FQBN    Fully Qualified Board Name (default: $DEFAULT_FQBN)
  -e, --erase         Full chip erase before flashing (useful after partition changes)
  -m, --monitor       Automatically start serial monitor after flashing
  -h, --help          Show this help

Environment variables:
  PORT, FQBN          Same as above (overridden by flags)

Examples:
  $0
  $0 /dev/ttyUSB0
  $0 -p /dev/ttyACM0 -b esp32:esp32:esp32s3 --monitor
  $0 --erase
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--port)     PORT="$2"; shift 2 ;;
    -b|--board)    FQBN="$2"; shift 2 ;;
    -e|--erase)    DO_ERASE=true; shift ;;
    -m|--monitor)  AUTO_MONITOR=true; shift ;;
    -h|--help)     show_help; exit 0 ;;
    *)             if [[ -z "$PORT" ]]; then PORT="$1"; else FQBN="$1"; fi; shift ;;
  esac
done

# ==================== COLOR HELPERS ====================

print_header() { echo -e "\n\033[1;36m=== $1 ===\033[0m"; }
print_success() { echo -e "\033[1;32m✓ $1\033[0m"; }
print_warning() { echo -e "\033[1;33m⚠ $1\033[0m"; }
print_error() { echo -e "\033[1;31m✗ $1\033[0m" >&2; }

command_exists() { command -v "$1" >/dev/null 2>&1; }

# ==================== SERIAL PORT AUTO-DETECTION ====================

detect_serial_port() {
  local os
  os=$(uname -s)
  local candidates=()

  case "$os" in
    Linux*)
      candidates=(/dev/ttyUSB* /dev/ttyACM* /dev/ttyAMA* /dev/serial/by-id/*)
      ;;
    Darwin*)
      candidates=(/dev/cu.usbserial* /dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial*)
      ;;
    MINGW*|CYGWIN*|MSYS*)
      candidates=(/dev/ttyS* COM*)
      ;;
    *)
      candidates=(/dev/ttyUSB* /dev/ttyACM*)
      ;;
  esac

  # Expand globs and keep only existing files
  local existing=()
  for pattern in "${candidates[@]}"; do
    for f in $pattern; do
      [[ -e "$f" ]] && existing+=("$f")
    done
  done

  if [[ ${#existing[@]} -eq 0 ]]; then
    return 1
  fi

  # Sort by modification time, newest first
  local sorted
  sorted=$(ls -t "${existing[@]}" 2>/dev/null | head -n 5)

  if [[ $(echo "$sorted" | wc -l) -eq 1 ]]; then
    echo "$sorted"
    return 0
  fi

  # Multiple ports found — pick the newest one automatically
  local chosen
  chosen=$(echo "$sorted" | head -n1)
  print_warning "Multiple serial ports detected. Using most recently connected: $chosen"
  echo "$chosen"
  return 0
}

# ==================== PRE-FLIGHT ====================

print_header "ESP32 CSI UDP Sender - Smart Headless Flasher v2"

echo "Sketch : $SKETCH"
echo "FQBN   : $FQBN"
echo

if ! command_exists arduino-cli; then
  print_error "arduino-cli not found"
  echo "Install: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
  exit 1
fi

if ! command_exists esptool.py && ! command_exists esptool; then
  print_error "esptool not found"
  echo "Install: pip install --upgrade esptool"
  exit 1
fi

ESPTool="esptool.py"
command_exists esptool && ESPTool="esptool"

if [[ ! -f "$SKETCH" ]]; then
  print_error "Sketch '$SKETCH' not found. Run from the esp32/ directory."
  exit 1
fi

# Auto-detect port if not provided
if [[ -z "$PORT" ]]; then
  if detected_port=$(detect_serial_port); then
    PORT="$detected_port"
    print_success "Auto-detected serial port: $PORT"
  else
    print_error "No serial port detected."
    echo "Please specify one:"
    echo "  $0 /dev/ttyUSB0"
    echo "  or set PORT=/dev/ttyUSB0"
    exit 1
  fi
fi

echo "Port   : $PORT"
echo "Build  : $BUILD_DIR"
if $DO_ERASE; then echo "Mode   : Full erase + flash"; fi
echo

# Linux permission hint
if [[ "$(uname -s)" == Linux* ]] && ! groups | grep -q dialout; then
  print_warning "You may need to be in the 'dialout' group for serial access."
  echo "  Run: sudo usermod -a -G dialout $USER && newgrp dialout"
fi

# ==================== COMPILE ====================

print_header "Compiling"

echo "arduino-cli compile --fqbn $FQBN --output-dir $BUILD_DIR $SKETCH"
if ! arduino-cli compile --fqbn "$FQBN" --output-dir "$BUILD_DIR" "$SKETCH"; then
  print_error "Compilation failed"
  exit 1
fi
print_success "Compilation successful"
ls -lh "$BUILD_DIR"/*.bin 2>/dev/null | head -n 5 || true

# ==================== OPTIONAL FULL ERASE ====================

if $DO_ERASE; then
  print_header "Full chip erase"
  echo "This will take ~10-30 seconds..."
  if ! $ESPTool --chip esp32 --port "$PORT" --baud "$FLASH_BAUD" erase_flash; then
    print_error "Erase failed (continuing anyway)"
  else
    print_success "Chip erased"
  fi
fi

# ==================== FLASH ====================

print_header "Flashing with $ESPTool"

echo "Target : $PORT @ ${FLASH_BAUD} baud"
echo "Offsets: 0x1000 (bootloader) + 0x8000 (partitions) + 0x10000 (app)"

echo

app_bin="$BUILD_DIR/${SKETCH%.ino}.ino.bin"
boot_bin="$BUILD_DIR/${SKETCH%.ino}.ino.bootloader.bin"
part_bin="$BUILD_DIR/${SKETCH%.ino}.ino.partitions.bin"

if ! $ESPTool --chip esp32 \
     --port "$PORT" \
     --baud "$FLASH_BAUD" \
     write_flash -z \
       0x1000  "$boot_bin" \
       0x8000  "$part_bin" \
       0x10000 "$app_bin"; then

  print_error "Flashing failed"
  echo ""
  echo "Troubleshooting tips:"
  echo "  • Put board in download mode: hold BOOT button, press RESET (or release BOOT)"
  echo "  • Try lower baud: --baud 115200 or 460800"
  echo "  • Check cable and permissions"
  echo "  • Run with --erase if you changed partition scheme"
  exit 1
fi

print_success "Flashing completed successfully!"

# ==================== POST-FLASH ====================

print_header "Post-flash"

echo "Your ESP32 CSI node should now be running."
echo "It will connect to WiFi and start sending JSON CSI packets to UDP port 4210."
echo

echo "Useful next commands:"
echo "  arduino-cli monitor --port $PORT --config baudrate=$MONITOR_BAUD"
if command_exists screen; then
  echo "  screen $PORT $MONITOR_BAUD"
fi

echo

# ==================== OPTIONAL MONITOR ====================

if $AUTO_MONITOR; then
  print_header "Starting serial monitor"
  if command_exists arduino-cli; then
    arduino-cli monitor --port "$PORT" --config baudrate=$MONITOR_BAUD
  elif command_exists screen; then
    screen "$PORT" $MONITOR_BAUD
  else
    print_warning "No monitor tool found. Install screen or use arduino-cli."
  fi
else
  read -r -p "Start serial monitor now? [y/N] " ans
  if [[ "$ans" =~ ^[Yy]$ ]]; then
    if command_exists arduino-cli; then
      arduino-cli monitor --port "$PORT" --config baudrate=$MONITOR_BAUD
    elif command_exists screen; then
      screen "$PORT" $MONITOR_BAUD
    else
      print_warning "Install 'screen' or arduino-cli for monitoring."
    fi
  fi
fi

print_success "All done. Happy sensing!"
