# ESP32 CSI Project

## Project Structure (PlatformIO)

```
esp32/
├── platformio.ini
├── src/
│   └── main.cpp          # Unified code for all boards
├── flash.sh
└── README.md
```

## Supported Boards

- **Standard ESP32** (WROOM-32UE, DevKit, etc.) → `env:esp32-standard`
- **Cheap Yellow Display** (ESP32-2432S028) → `env:esp32-cyd`

## Flashing

```bash
./flash.sh --standard
./flash.sh --cyd
./flash.sh --cyd --monitor
```

The code in `src/main.cpp` uses `#if HAS_DISPLAY` to include or exclude display code at compile time.

## Key Features

- WiFiManager for configuration
- Real CSI collection
- Unified code for multiple hardware platforms
- Proper `src/main.cpp` structure for PlatformIO
