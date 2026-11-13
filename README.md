# Project Starbeam V2 - Custom Fork

This is a customized fork of the original [Project Starbeam V2](https://github.com/dkyazzentwatwa/project-starbeam) project.

> [!WARNING]
> **IMPORTANT: V2 ONLY**
> This firmware and all modifications described here apply **exclusively** to the **Starbeam V2** hardware. It is not compatible with V1.

> [!IMPORTANT]
> **HARDWARE CONFIGURATION**
> This build is optimized for a setup with **3× NRF24 modules** (on VSPI) and **2× CC1101 modules** (on HSPI). NRF24 radios 4 and 5 are disabled in `config.h` because the HSPI bus is dedicated to the CC1101 modules.

> [!TIP]
> **HARDWARE MODIFICATION: DNF RESISTORS NOT NEEDED**
> Thanks to improved software-level SPI bus management, the **DNF (Do Not Fit) resistors are no longer required**. You can leave them unpopulated.

---

## Table of Contents

- [Quick Start](#quick-start)
- [Changes and Improvements](#changes-and-improvements)
- [Complete API Reference](#complete-api-reference)
  - [Scan Endpoints](#scan-endpoints)
  - [RF Signal Endpoints (433 MHz)](#rf-signal-endpoints-433-mhz)
  - [Attack Endpoints (WiFi Security Testing)](#attack-endpoints-wifi-security-testing)
  - [PMKID Endpoints](#pmkid-endpoints)
- [API Examples](#api-examples)
- [Important Notes & Limitations](#important-notes--limitations)
- [Installation & Usage](#installation--usage)
- [Development Notes](#development-notes)
- [License](#license)

---

## Quick Start

### Compile and Upload

Requires the `huge_app` partition scheme:

```bash
arduino-cli compile --upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app -p /dev/cu.SLAB_USBtoUART .
```

*Replace `/dev/cu.SLAB_USBtoUART` with your ESP32 port (Linux: `/dev/ttyUSB0`, Windows: `COM3`).*

### Required Libraries

- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `U8g2_for_Adafruit_GFX`
- `RF24`
- `SmartRC-CC1101-Driver-Lib` (**version 2.5.7**)

---

## Changes and Improvements

### 1. WiFi Station (STA) Mode & Auto-Start Web Server
- **Auto-Start:** The web server now starts automatically on boot; no manual activation is needed.
- **3-Network Capacity:** ESP32 attempts to connect to up to 3 pre-configured WiFi networks on boot (configured in `config.h`).
- **Smart Fallback:** If none of the 3 networks are found, the ESP32 automatically falls back to Access Point (AP) mode (SSID: `Starbeam`) so you are never locked out.
- **Menu Toggle:** New **WiFi Mode** menu option to switch between STA mode (connect to router) and AP mode on the fly.
- **IP Display:** OLED screen shows the active IP address on boot.

### 2. Hardware & GPIO Fixes (Input-Only Pins)
- **GPIO35 (CC1101 #2 GDO0):** The ESP32 does not allow pins 34–39 to be configured as `OUTPUT`. The `SmartRC_CC1101` library has been patched so `GDO0` is no longer configured as output, allowing the second CC1101 module to initialize correctly without crashing.
- **GPIO34/39 (Buttons):** `INPUT_PULLUP` mode has been removed from `analyzer.cpp`. These input-only pins do not have internal pull-up resistors, which was causing the `gpio_pullup_en(85)` error. They now rely on the hardware's external pull-ups.

### 3. NRF24 & WiFi Conflict Fix
- **WiFi Hardware Preservation:** In `analyzer.cpp`, `esp_wifi_stop()` and `esp_wifi_deinit()` were previously called, destroying the ESP32's WiFi radio and breaking the WiFi Scanner and Web Server. These calls have been removed.
- **Disabled NRF24 Modules:** NRF24 modules 4 and 5 are disabled (`-1`) because the HSPI bus is dedicated to the two CC1101 modules. The HSPI Test now tests the CC1101s instead of the absent NRF24s.

### 4. UI & Menu Improvements
- **Button Debounce:** A "wait for release" mechanism has been added to the main `loop()`, preventing re-triggering of a menu item if the `SELECT` button is held while exiting a feature.
- **Menu Alignment:** `menuLabels` in `display.cpp` are aligned with the `MenuItem` enum in `types.h`, eliminating empty spaces.

### 5. Async WiFi & BLE Scans (NEW)
- **Non-Blocking:** The web interface now uses `WiFi.scanNetworks(true)` for background scanning, keeping the Access Point reachable during the scan.
- **Polling Pattern:** JavaScript polls `/api/wifi` every second until the scan completes.
- **Sync Endpoints:** For scripts, `/api/wifi/sync` and `/api/ble/sync` perform a complete scan in a single request.

### 6. NRF24 Analyzer Web Page (NEW)
- **Live Spectrum:** New `/nrf` web page shows real-time 2.4 GHz spectrum activity using the NRF24 modules.
- **Auto-Refresh:** Updates every second via `/api/nrf` JSON endpoint.
- **Visual Bar Graph:** Color-coded signal strength (green/orange/red).

### 7. RF Signal API for 433 MHz (NEW)
- **Frequency Scan:** `/api/rf/scan` sweeps a frequency range and returns RSSI peaks (great for finding unknown remote frequencies).
- **Raw Capture:** `/api/rf/capture` captures raw edge timings from a 433 MHz signal (remote controls, sensors, etc.).
- **Replay:** `/api/rf/replay` re-transmits the captured signal.
- **Info & Clear:** `/api/rf/info` and `/api/rf/clear` for capture management.

---

## Complete API Reference

All endpoints are accessible via `http://<IP>/`.

### Scan Endpoints

| Endpoint | Method | Description |
| :--- | :--- | :--- |
| `/api/wifi?scan=1` | `GET` | Start async WiFi scan |
| `/api/wifi` | `GET` | Poll WiFi scan results (`status`: `scanning` / `done`) |
| `/api/wifi/sync` | `GET` | One-shot WiFi scan (blocks 2–4s) |
| `/api/ble?scan=1` | `GET` | Start async BLE scan |
| `/api/ble` | `GET` | Poll BLE scan results |
| `/api/ble/sync` | `GET` | One-shot BLE scan |
| `/api/nrf` | `GET` | Live NRF24 spectrum data |
| `/api/scan/status` | `GET` | Current scan state |

### RF Signal Endpoints (433 MHz)

| Endpoint | Parameters | Description |
| :--- | :--- | :--- |
| `/api/rf/scan` | `start`, `end`, `step` (MHz) | Sweep frequency range, returns RSSI peaks |
| `/api/rf/capture` | `freq` (MHz), `duration` (ms) | Capture raw signal edges |
| `/api/rf/replay` | *none* | Re-transmit last capture |
| `/api/rf/info` | *none* | Info about last capture |
| `/api/rf/clear` | *none* | Clear capture buffer |

### Attack Endpoints (WiFi Security Testing)

| Endpoint | Method | Parameters |
| :--- | :--- | :--- |
| `/api/attack/deauth` | `POST` | `network` (index or `-1` for broadcast), `reason` |
| `/api/attack/beacon` | `POST` | *none* |
| `/api/attack/probe` | `POST` | *none* |
| `/api/attack/pmkid` | `POST` | `network` (index) |
| `/api/attack/stop` | `POST` | *none* |
| `/api/attack/status` | `GET` | *none* |

### PMKID Endpoints

| Endpoint | Method | Description |
| :--- | :--- | :--- |
| `/api/pmkid/list` | `GET` | JSON list of captured PMKIDs |
| `/api/pmkid/download` | `GET` | Hashcat-format text file |
| `/api/pmkid/clear` | `POST` | Clear PMKID history |

---

## API Examples

### WiFi Scan (Async, Two-Step)
```bash
# Start scan
curl "http://192.168.4.1/api/wifi?scan=1"
# {"status":"scanning"}

# Poll (repeat until status="done")
curl "http://192.168.4.1/api/wifi"
# {"status":"done","count":5,"networks":[...]}
```

### WiFi Scan (Sync, Single Request)
```bash
curl "http://192.168.4.1/api/wifi/sync" | jq '.networks[].ssid'
```

### NRF24 Spectrum
```bash
curl "http://192.168.4.1/api/nrf"
# {"channel":42,"signal":3}
```

### 433 MHz Signal Scan
Hold the remote button while scanning:
```bash
# Sweep 430-440 MHz to find the remote's frequency
curl "http://192.168.4.1/api/rf/scan?start=430&end=440&step=0.1" | jq '.peak_freq, .peak_rssi'
# 433.92
# -62.0
```

### 433 MHz Signal Capture
Hold the remote button during capture:
```bash
curl "http://192.168.4.1/api/rf/capture?freq=433.92&duration=1000" | jq '.count, .signal_duration_us'
# 87
# 45230
```

### 433 MHz Signal Replay
```bash
curl "http://192.168.4.1/api/rf/replay"
# {"status":"replayed"}
```

### WiFi Attacks & PMKID
```bash
# Targeted deauth
curl -L -X POST -d "network=3&reason=7" "http://192.168.4.1/api/attack/deauth"

# Broadcast deauth
curl -L -X POST -d "network=-1&reason=7" "http://192.168.4.1/api/attack/deauth"

# Beacon flood
curl -L -X POST "http://192.168.4.1/api/attack/beacon"

# Stop attack
curl -X POST "http://192.168.4.1/api/attack/stop"

# Download PMKIDs
curl "http://192.168.4.1/api/pmkid/download" > pmkids.txt
```

---

## Important Notes & Limitations

### Access Point Availability During Operations
The ESP32 has a single WiFi radio and cannot simultaneously serve the Access Point, perform a WiFi scan, and execute a WiFi attack:

| Operation | AP Impact |
| :--- | :--- |
| **Async WiFi/BLE scan** | ✅ AP stays reachable (background scan) |
| **Sync WiFi scan** | ⚠️ AP blocked for 2–4 seconds |
| **NRF24 spectrum** | ✅ No impact (uses NRF24 hardware) |
| **RF scan/capture** | ✅ No impact (uses CC1101 hardware) |
| **WiFi attacks (deauth, beacon, etc.)** | ⚠️ AP may become unreachable during attack |

> [!TIP]
> Start WiFi attacks from the OLED menu if you need to keep the web server available, and use async scans for monitoring.

### Known Limitations
- `/api/wifi/sync` and `/api/ble/sync` block the web server for the duration of the scan (2–4 seconds for WiFi, less than 1 second for BLE).
- WiFi attacks may temporarily disrupt your own Access Point connection. This is a hardware limitation of the ESP32, not a software bug.
- RF replay requires the same modulation and timing as the original signal. Some remotes use rolling codes or FSK modulation that may not replay correctly.

---

## Installation & Usage

1. Ensure `SmartRC-CC1101-Driver-Lib` (**version 2.5.7**) is installed.
2. Edit `config.h` to add your WiFi SSIDs and passwords (up to 3 networks).
3. Compile and upload with the `huge_app` partition scheme.
4. The web server starts automatically. Connect to the `Starbeam` AP (if fallback is active) or your router's network.
5. Navigate to the IP address shown on the OLED screen (e.g., `192.168.4.1`).

### Web Interface Pages

| Page | Purpose |
| :--- | :--- |
| `/` | Dashboard with scan status |
| `/wifi` | WiFi network scanner |
| `/ble` | BLE device scanner |
| `/nrf` | NRF24 2.4 GHz live spectrum |
| `/security` | WiFi security testing tools |

---

## Development Notes

### Configuration File (`config.h`)
```cpp
#define MAX_STA_NETWORKS 3

static const char* const STA_SSIDS[MAX_STA_NETWORKS] = {
    "YourNetwork1",
    "YourNetwork2",
    "YourNetwork3"
};

static const char* const STA_PASSWORDS[MAX_STA_NETWORKS] = {
    "password1",
    "password2",
    "password3"
};
```

### Adding New Menu Items
1. Add state to `AppState` enum in `types.h`.
2. Add item to `MenuItem` enum in `types.h` *(order matters!)*.
3. Add label to `menuLabels[]` in `display.cpp` *(same position as in `types.h`)*.
4. Add case in `executeSelectedMenuItem()` in `app_main.cpp`.

### Memory Usage
- **Program:** ~1.81 MB / 3 MB (with `huge_app` partition)
- **Free heap after setup:** greater than 200 KB
- **RF capture buffer:** 512 edges × 5 bytes = 2.5 KB

---

## License

Original Project Starbeam by [dkyazzentwatwa](https://github.com/dkyazzentwatwa/project-starbeam).
This fork maintains the original licensing terms. See the `LICENSE` file for details.

*Fork maintained by [raspiuser1](https://github.com/raspiuser1).*
