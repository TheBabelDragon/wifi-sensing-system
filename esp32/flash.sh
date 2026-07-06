#!/usr/bin/env bash
#
# flash.sh - Smart headless ESP32 CSI UDP Sender flasher
# Auto-detects serial port + chip type, smart FQBN selection, nice UX.
#
# Usage examples:
#   ./flash.sh
#   ./flash.sh --monitor
#   ./flash.sh -p /dev/ttyACM0 --erase
#   ./flash.sh -b esp32:esp32:esp32s3
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
DETECTED_CHIP=""

# ==================== ARGUMENT PARSING ====================

show_help() {
  cat <<EOF
ESP32 CSI UDP Sender - Smart Headless Flashing Helper

Usage: $0 [options] [PORT]

Options:
  -p, --port PORT     Serial port (auto-detected if omitted)
  -b, --board FQBN    Force specific Fully Qualified Board Name
  -e, --erase         Full chip erase before flashing
  -m, --monitor       Auto-start serial monitor after flashing
  -h, --help          Show this help

The script auto-detects the connected ESP32 chip type (S3, C3, etc.)
and chooses a sensible FQBN unless you override with -b.

Examples:
  $0
  $0 --monitor
  $0 -p /dev/ttyUSB0 --erase
  $0 -b esp32:esp32:esp32s3
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--port)     PORT="$2"; shift 2 ;;
    -b|--board)    FQBN="$2"; shift 2 ;;
    -e|--erase)    DO_ERASE=true; shift ;;
    -m|--monitor)  AUTO_MONITOR=true; shift ;;
    -h|--help)     show_help; exit 0 ;;
    *)             if [[ -z "$PORT" ]]; then PORT="$1"; fi; shift ;;
  esac
done

# ==================== COLOR & HELPER FUNCTIONS ====================

print_header() { echo -e "\n\033[1;36m=== $1 ===\033[0m"; }
print_success() { echo -e "\033[1;32m✓ $1\033[0m"; }
print_warning() { echo -e "\033[1;33m⚠ $1\033[0m"; }
print_error() { echo -e "\033[1;31m✗ $1\033[0m" >&2; }

command_exists() { command -v "$1" >/dev/null 2>&1; }

# Map detected chip to recommended FQBN
get_recommended_fqbn() {
  local chip="$1"
  case "$chip" in
    ESP32)          echo "esp32:esp32:esp32dev" ;;
    ESP32-S2)       echo "esp32:esp32:esp32s2" ;;
    ESP32-S3)       echo "esp32:esp32:esp32s3" ;;
    ESP32-C3)       echo "esp32:esp32:esp32c3" ;;
    ESP32-C6)       echo "esp32:esp32:esp32c6" ;;
    ESP32-P4)       echo "esp32:esp32:esp32p4" ;;   # May need arduino-esp32 >= 3.0+
    *)              echo "$DEFAULT_FQBN" ;;
  esac
}

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

  local existing=()
  for pattern in "${candidates[@]}"; do
    for f in $pattern; do
      [[ -e "$f" ]] && existing+=("$f")
    done
  done

  [[ ${#existing[@]} -eq 0 ]] && return 1

  local sorted
  sorted=$(ls -t "${existing[@]}" 2>/dev/null | head -n 5)

  if [[ $(echo "$sorted" | wc -l) -eq 1 ]]; then
    echo "$sorted"
  else
    local chosen
    chosen=$(echo "$sorted" | head -n1)
    print_warning "Multiple ports found. Using newest: $chosen"
    echo "$chosen"
  fi
  return 0
}

# ==================== CHIP TYPE DETECTION ====================

detect_chip_type() {
  local port="$1"
  local chip_output

  # Try to get chip info (works with both esptool.py and esptool)
  if chip_output=$($ESPTool --port "$port" --baud 115200 chip_id 2>/dev/null); then
    # Parse output like "Chip is ESP32-S3"
    if echo "$chip_output" | grep -q "Chip is ESP32-S3"; then
      echo "ESP32-S3"
    elif echo "$chip_output" | grep -q "Chip is ESP32-C3"; then
      echo "ESP32-C3"
    elif echo "$chip_output" | grep -q "Chip is ESP32-C6"; then
      echo "ESP32-C6"
    elif echo "$chip_output" | grep -q "Chip is ESP32-S2"; then
      echo "ESP32-S2"
    elif echo "$chip_output" | grep -q "Chip is ESP32-P4"; then
      echo "ESP32-P4"
    elif echo "$chip_output" | grep -q "Chip is ESP32"; then
      echo "ESP32"
    else
      echo "Unknown"
    fi
  else
    echo "Unknown"
  fi
}

# ==================== PRE-FLIGHT ====================

print_header "ESP32 CSI UDP Sender - Smart Headless Flasher v3"

echo "Sketch : $SKETCH"
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
  print_error "Sketch not found. Run from esp32/ directory."
  exit 1
fi

# === PORT DETECTION ===
if [[ -z "$PORT" ]]; then
  if detected_port=$(detect_serial_port); then
    PORT="$detected_port"
    print_success "Auto-detected port: $PORT"
  else
    print_error "No serial port detected. Specify with -p PORT or set PORT=..."
    exit 1
  fi
fi

echo "Port   : $PORT"

# === CHIP TYPE DETECTION + SMART FQBN ===
if [[ -z "$FQBN" || "$FQBN" == "$DEFAULT_FQBN" ]]; then
  DETECTED_CHIP=$(detect_chip_type "$PORT")
  if [[ "$DETECTED_CHIP" != "Unknown" && -n "$DETECTED_CHIP" ]]; then
    local recommended
    recommended=$(get_recommended_fqbn "$DETECTED_CHIP")
    if [[ "$recommended" != "$DEFAULT_FQBN" ]]; then
      FQBN="$recommended"
      print_success "Detected chip: $DETECTED_CHIP → using FQBN: $FQBN"
    else
      print_warning "Detected chip: $DETECTED_CHIP (using default FQBN)"
    fi
  else
    print_warning "Could not auto-detect chip type (using default FQBN: $FQBN)"
    print_warning "You can force it with: -b esp32:esp32:esp32s3  (or similar)"
  fi
else
  print_warning "Using user-specified FQBN: $FQBN (chip auto-detection skipped)"
fi

echo "FQBN   : $FQBN"
echo "Build  : $BUILD_DIR"
if $DO_ERASE; then echo "Mode   : Full erase + flash"; fi
echo

# Linux permission hint
if [[ "$(uname -s)" == Linux* ]] && ! groups | grep -q dialout; then
  print_warning "You may need 'dialout' group access."
  echo "  sudo usermod -a -G dialout $USER && newgrp dialout"
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
  echo "This will take a while..."
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
  echo "Troubleshooting:"
  echo "  • Hold BOOT button while pressing RESET to enter download mode"
  echo "  • Try lower baud rate or different cable"
  echo "  • Use --erase if you changed partition scheme"
  exit 1
fi

print_success "Flashing completed successfully!"

# ==================== POST-FLASH ====================

print_header "Post-flash"

echo "ESP32 CSI node is now running."
echo "It will connect to WiFi and stream JSON CSI data to UDP port 4210."
echo

echo "Monitor with:"
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
    print_warning "No serial monitor tool found."
  fi
else
  read -r -p "Start serial monitor now? [y/N] " ans
  if [[ "$ans" =~ ^[Yy]$ ]]; then
    if command_exists arduino-cli; then
      arduino-cli monitor --port "$PORT" --config baudrate=$MONITOR_BAUD
    elif command_exists screen; then
      screen "$PORT" $MONITOR_BAUD
    fi
  fi
fi

print_success "All done. Happy sensing!"
