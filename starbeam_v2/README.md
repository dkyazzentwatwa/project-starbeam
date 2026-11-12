# Starbeam V2 - Modular Firmware

This is the **V2 modular firmware** for Project Starbeam - a complete refactoring of the V1 monolithic codebase into a clean, maintainable architecture.

## TL;DR - One Command to Rule Them All

```bash
# Compile and upload (requires huge_app partition for security features)
arduino-cli compile --upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app -p /dev/cu.SLAB_USBtoUART .
```

Replace `/dev/cu.SLAB_USBtoUART` with your ESP32 port (Linux: `/dev/ttyUSB0`, Windows: `COM3`)

## Quick Start

### Prerequisites

- **Arduino IDE 2.x** or **Arduino CLI** (CLI recommended for automation)
- **ESP32 Board Support**: `https://dl.espressif.com/dl/package_esp32_index.json`
- **Required Libraries**: Adafruit GFX, Adafruit SSD1306, U8g2_for_Adafruit_GFX, RF24, ELECHOUSE_CC1101_SRC_DRV

### Compilation & Upload with Arduino CLI

**IMPORTANT**: This firmware **requires** the `huge_app` partition scheme (3MB for program) to fit all features including WiFi security testing and web server. The default partition scheme (1.2MB) is insufficient.

```bash
# First-time setup: Install Arduino CLI
# macOS:
brew install arduino-cli

# Linux:
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Windows: Download from https://arduino.github.io/arduino-cli/

# Install ESP32 platform (first time only)
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Install required libraries (first time only)
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "U8g2_for_Adafruit_GFX"
arduino-cli lib install "RF24"
arduino-cli lib install "ELECHOUSE_CC1101_SRC_DRV"

# Find your ESP32 port
ls /dev/cu.*        # macOS
ls /dev/ttyUSB*     # Linux
# Windows: Check Device Manager (COMx)

# Compile and upload in one command (recommended)
arduino-cli compile --upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app -p /dev/cu.SLAB_USBtoUART .

# Or compile first, then upload separately
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app .
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app -p /dev/cu.SLAB_USBtoUART .

# Monitor serial output (115200 baud)
screen /dev/cu.SLAB_USBtoUART 115200
# Or: arduino-cli monitor -p /dev/cu.SLAB_USBtoUART -c baudrate=115200
```

**Common port names**:
- macOS: `/dev/cu.SLAB_USBtoUART`, `/dev/cu.usbserial-*`, `/dev/cu.wchusbserial*`
- Linux: `/dev/ttyUSB0`, `/dev/ttyACM0`
- Windows: `COM3`, `COM4`, `COM5` (check Device Manager)

### Using Arduino IDE

1. Open `starbeam_v2.ino`
2. Tools → Board → ESP32 Arduino → ESP32 Dev Module
3. **Tools → Partition Scheme → Huge APP (3MB No OTA/1MB SPIFFS)** ← **REQUIRED**
4. Tools → Upload Speed → 921600
5. Tools → Port → Select your ESP32 port
6. Sketch → Upload (Ctrl+U / Cmd+U)

If upload fails, hold the **BOOT** button on the ESP32 during the "Connecting..." phase.

## Architecture Overview

### File Structure

```
starbeam_v2/
├── starbeam_v2.ino          # Main firmware, state machine, menu system
├── config.h                  # Hardware pins, buffer sizes, timing constants
├── types.h                   # Enums (AppState, MenuItem), structs
└── src/
    ├── display.h/cpp         # OLED display interface
    ├── input.h/cpp           # Button handling with debouncing
    ├── terminal.h/cpp        # Serial CLI interface (headless operation)
    ├── nrf24.h/cpp           # NRF24L01 radio management (5 radios)
    ├── cc1101.h/cpp          # CC1101 radio control (2 radios)
    ├── analyzer.h/cpp        # NRF24 spectrum analyzer
    ├── recording.h/cpp       # Signal capture/replay
    ├── wifi_scanner.h/cpp    # WiFi network enumeration
    ├── ble_scanner.h/cpp     # BLE device discovery
    ├── webserver.h/cpp       # Web interface with captive portal
    └── wifi_attack.h/cpp     # WiFi security testing framework
```

### Key Differences from V1

| Aspect | V1 | V2 |
|--------|----|----|
| **Structure** | Monolithic (~1000+ lines) | Modular (11+ files) |
| **Configuration** | Hardcoded in main file | Centralized in config.h |
| **Types** | Scattered definitions | Centralized in types.h |
| **Radio Control** | Inline code | Dedicated modules |
| **Terminal/CLI** | Debug output only | Full command interface + display echo |
| **Web Interface** | Basic HTML | Full REST API + captive portal |
| **WiFi Attacks** | Not implemented | Full framework with 5 modes |
| **Maintainability** | Difficult | Easy to extend |

## Hardware Configuration

All hardware configuration is in `config.h`:

- **NRF24 Radios 1-3**: VSPI bus (CE=27/26/25, CS=15/33/5)
- **NRF24 Radios 4-5**: HSPI bus (CE=4/32, CS=2/17)
- **CC1101 Radios 1-2**: HSPI bus (SS=2/32, GDO0=4/35, GDO2=16/17)
- **Display**: SSD1306 OLED, I2C @ 0x3C
- **Buttons**: Up=39, Down=34, Select=36

## Features

### Core Capabilities

- **RF Jamming**: Bluetooth, WiFi, Drone, CC1101 (authorized use only)
- **RF Analysis**: NRF24 spectrum scanner, CC1101 frequency scanner
- **Signal Recording**: Capture and replay RF signals
- **Network Scanning**: WiFi networks, BLE devices, channel heatmaps

### WiFi Security Testing (NEW in V2)

**⚠️ REQUIRES AUTHORIZATION - Educational/Research Use Only**

- **Deauthentication**: Targeted (specific client) or broadcast (all networks)
- **Beacon Flooding**: Generate up to 20 fake access points
- **Probe Request Flooding**: Simulate multiple client devices
- **PMKID Capture**: Extract WPA2/WPA3 handshake data

### Web Interface (Enhanced in V2)

Access via `http://192.168.4.1` after connecting to `StarbeamAP`:

- WiFi Scanner Dashboard (live updates)
- BLE Scanner Dashboard (live updates)
- Security Testing Dashboard (attack control)
- JSON APIs for remote monitoring

**Endpoints**:
- `/api/wifi` - WiFi scan results
- `/api/ble` - BLE scan results
- `/api/attack/status` - Attack status
- `/security` - Security testing interface

### Terminal/CLI Mode (NEW in V2)

**Headless Operation** - Control Starbeam via serial terminal without physical buttons or display!

Connect via USB serial at 115200 baud and type commands directly:

```bash
# Connect to ESP32
screen /dev/cu.SLAB_USBtoUART 115200
# Or: arduino-cli monitor -p /dev/cu.SLAB_USBtoUART -c baudrate=115200

# You'll see the terminal prompt
> help
```

**Key Features**:
- **36 Commands**: All menu items accessible via typed commands (e.g., `wifi_scan`, `bt_jam`, `deauth_target`)
- **Display Echo**: OLED screen content mirrored to terminal (toggle with `echo on/off`)
- **Stop Command**: Interrupt any running operation with `stop`
- **Help System**: Type `help` to see all available commands
- **Status Info**: Type `status` for system information
- **Last Input Wins**: Buttons and terminal both work - whichever is used last takes control

**Command Examples**:

```bash
> help                # Show all 36 commands with descriptions
> status              # Display system status (heap, uptime, settings)
> menu                # List all menu items with numbers

> wifi_scan           # Scan for WiFi networks
> ble_scan            # Scan for BLE devices
> bt_jam              # Start Bluetooth jammer
> stop                # Stop current operation

> echo off            # Disable display echo to terminal
> echo on             # Enable display echo (default)
> verbose on          # Enable verbose logging
```

**All 36 Available Commands**:

| Command | Description |
|---------|-------------|
| `bt_jam` | Bluetooth jammer (2.4GHz) |
| `drone_jam` | Drone jammer |
| `wifi_jam` | WiFi jammer (2.4GHz) |
| `cc1_jam` | 433MHz jammer (both radios) |
| `cc_scan` | CC1101 frequency scan |
| `nrf_scan` | NRF24 spectrum analyzer |
| `wifi_scan` | WiFi network scanner |
| `wifi_heatmap` | WiFi channel heatmap |
| `ble_scan` | BLE device scanner |
| `web_on` | Start web server |
| `web_off` | Stop web server |
| `web_status` | Web server status |
| `deauth_target` | Targeted deauth attack |
| `deauth_all` | Broadcast deauth attack |
| `beacon_flood` | Beacon flooding attack |
| `probe_flood` | Probe request flooding |
| `pmkid` | PMKID capture |
| `test_nrf` | Test NRF24 radios (VSPI) |
| `test_cc` | Test CC1101 radios |
| `test_hspi` | Test NRF24 radios (HSPI) |
| `cc1_single` | CC1101 #1 only jammer |
| `cc2_single` | CC1101 #2 only jammer |
| `rec_raw` | Record raw 433MHz signal |
| `play_raw` | Replay raw 433MHz signal |
| `show_raw` | Show raw recording data |
| `show_buff` | Show buffer contents |
| `get_rssi` | Get CC1101 RSSI value |
| `flush_buff` | Clear recording buffer |
| `stop_all` | Stop all operations |
| `reset_cc` | Reset CC1101 radios |
| `freq_43440` | Set frequency 434.40MHz |
| `freq_43430` | Set frequency 434.30MHz |
| `freq_43400` | Set frequency 434.00MHz |
| `freq_43390` | Set frequency 433.90MHz |
| `settings` | Settings menu |
| `help` | Show help |

**Special Commands** (not in menu):
- `help` or `?` - Show full command reference
- `status` - System status (heap, uptime, settings)
- `menu` - List all 36 menu items
- `stop` - Interrupt current operation
- `echo on/off` - Toggle display echo
- `verbose on/off` - Toggle verbose logging

**Typical Workflow**:

```bash
# Connect via serial terminal
> status                    # Check system status
Free Heap:     245832 bytes
Uptime:        42 seconds
Display Echo:  ON
Verbose:       OFF

> wifi_scan                 # Start WiFi scan
Serial command - exiting current operation
Selected menu item: 6
WIFI_SCAN selected

[WiFi scan results appear both on OLED and in terminal]

> stop                      # Stop scanning
Stop signal sent

> bt_jam                    # Switch to BT jamming
Serial command - exiting current operation
Selected menu item: 0
BT JAM selected

> stop                      # Stop jamming
```

**Implementation Details**:
- Non-blocking serial input processing
- Command buffer: 128 characters
- Echo enabled by default
- Backspace/delete support
- Case-insensitive command parsing
- Module: `src/terminal.h/cpp` (~600 lines)

## Development Guide

### Adding New Menu Items

1. **Add state** to `AppState` enum in `types.h`:
   ```cpp
   enum AppState {
       // ... existing states
       STATE_MY_NEW_FEATURE,
   };
   ```

2. **Add menu item** to `MenuItem` enum in `types.h`:
   ```cpp
   enum MenuItem {
       // ... existing items
       MY_NEW_FEATURE,
   };
   ```

3. **Implement handler** in `starbeam_v2.ino`:
   ```cpp
   void loop() {
       switch (currentState) {
           // ... existing cases
           case STATE_MY_NEW_FEATURE:
               handleMyNewFeature();
               break;
       }
   }
   ```

4. **Add to menu system** in main file menu rendering

### Adding New Modules

1. **Create header** (`src/my_module.h`):
   ```cpp
   #ifndef MY_MODULE_H
   #define MY_MODULE_H

   class MyModule {
   public:
       static void init();
       static void doSomething();
   };

   #endif
   ```

2. **Create implementation** (`src/my_module.cpp`):
   ```cpp
   #include "my_module.h"

   void MyModule::init() {
       // Initialization code
   }

   void MyModule::doSomething() {
       // Feature code
   }
   ```

3. **Include in main file**:
   ```cpp
   #include "src/my_module.h"
   ```

4. **Initialize in setup()**:
   ```cpp
   void setup() {
       // ... other init
       MyModule::init();
   }
   ```

### Modifying Hardware Configuration

Edit `config.h` to change:

- Pin assignments (NRF24_X_CE, NRF24_X_CS, etc.)
- Buffer sizes (CCBUFFERSIZE, RECORDINGBUFFERSIZE)
- Timing constants (DEBOUNCE_MS, DISPLAY_REFRESH_MS)
- Task configuration (stack sizes, priorities, core assignment)

### Memory Management

V2 uses **static buffer allocation** to prevent heap fragmentation:

- `ccSendBuffer[64]` - CC1101 transmission buffer
- Attack frame buffers in `wifi_attack.cpp` (128-256 bytes)
- Display buffers managed by Adafruit library

**Monitor heap usage**:
```cpp
Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
```

Target: >200 KB free heap during operation

## Testing

### Compilation Test (No Hardware)

```bash
cd ..  # Go to project root
./test_build.sh
```

Expected output:
```
✓ COMPILATION SUCCESSFUL
Binary size: 1.81 MB (58%)
Sketch uses ~892KB flash, ~45KB RAM
```

### Serial Debug Output

Enable verbose logging at 115200 baud to see:

- Boot sequence
- Module initialization status
- WiFi/BLE scan results with MAC addresses
- Attack statistics (frames sent, clients disconnected)
- PMKID data in hashcat format
- RSSI measurements

### Common Issues

**Compilation fails with missing libraries**:
```bash
# Install via Arduino CLI
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "U8g2_for_Adafruit_GFX"
arduino-cli lib install "RF24"
arduino-cli lib install "ELECHOUSE_CC1101_SRC_DRV"
```

**Upload fails**:
- Hold BOOT button during upload
- Try slower upload speed: 460800 instead of 921600
- Verify correct port selection

**Out of memory**:
- Use partition scheme `huge_app` (3MB for code)
- Check heap usage with `ESP.getFreeHeap()`
- Reduce buffer sizes in `config.h` if needed

**WiFi attacks not working**:
- Ensure `huge_app` partition scheme is selected
- Check serial monitor for error messages
- Verify WiFi initialization completed successfully

## Performance Metrics

**Flash & RAM** (V2):
- Program Storage: 1.81 MB / 3 MB (58%)
- Dynamic Memory: ~70 KB / 320 KB (21%)
- Free Heap: >200 KB during operation

**Attack Performance**:
- Beacon Flood: >100 beacons/second
- Probe Flood: >500 probes/second
- Deauth (Targeted): Real-time client elimination
- Deauth (Broadcast): Full channel coverage (1-13)

**Boot Time**: ~2.5 seconds

## Legal Notice

**⚠️ CRITICAL**: This firmware includes WiFi security testing features that are **strictly regulated** by law.

**Authorized Use Only**:
- Penetration testing with signed contract
- Educational research in controlled lab
- Security audits with client authorization
- Your own personal network

**Prohibited Use**:
- Public WiFi networks (coffee shops, airports, etc.)
- Any network without explicit written permission
- Malicious or illegal activity

Violating computer crime laws can result in criminal prosecution, civil liability, and imprisonment. Users are solely responsible for compliance with all applicable laws including:

- **USA**: Computer Fraud and Abuse Act (CFAA), FCC Part 15
- **UK**: Computer Misuse Act 1990, Wireless Telegraphy Act 2006
- **EU**: GDPR, national computer crime laws

See main README.md for complete legal disclaimer.

## Resources

- **Main Documentation**: See `../README.md` and `../CLAUDE.md`
- **Testing Guide**: See `../TESTING.md`
- **Hardware Design**: See `../hardware/starbeam_V1_design/`

## License

Apache License 2.0 - See `../LICENSE` file for details.
