# Project Starbeam V2 - Custom Fork

This is a customized fork of the original [Project Starbeam V2](https://github.com/dkyazzentwatwa/project-starbeam) project.

This fork maintains 100% backward compatibility with the original hardware but adds new functionality and resolves several specific hardware and software issues encountered during testing.

## 🛠️ Changes and Improvements

### 1. WiFi Station (STA) Mode & Web Server
*   **Auto-Start:** The web server now starts automatically on boot (in `setup()`) instead of requiring manual activation from the menu.
*   **3-Network Capacity:** The ESP32 attempts to connect to up to 3 pre-configured WiFi networks on boot (configurable in `config.h`).
*   **Smart Fallback:** If none of the 3 networks are found, the ESP32 automatically falls back to Access Point (AP) mode (broadcasting the "Starbeam" network) so you are never locked out.
*   **Menu Toggle:** A new `WIFI_STA_MODE` menu option has been added to switch between STA mode (connecting to a router) and AP mode on the fly.
*   **IP Display:** The OLED screen now displays the active IP address on boot.

### 2. Hardware & GPIO Fixes (Input-Only Pins)
*   **GPIO35 (CC1101 #2 GDO0):** The ESP32 does not allow pins 34-39 to be configured as `OUTPUT`. The `SmartRC_CC1101` library has been patched so `GDO0` is no longer configured as an output, allowing the second CC1101 module (on GPIO35) to initialize correctly without crashing.
*   **GPIO34/39 (Buttons):** The `INPUT_PULLUP` mode has been removed from `analyzer.cpp`. These input-only pins do not have internal pull-up resistors, which was causing the `gpio_pullup_en(85)` error. They now rely on the hardware's external pull-up resistors.

### 3. NRF24 & WiFi Conflict Fix
*   **WiFi Hardware Preservation:** In `analyzer.cpp`, `esp_wifi_stop()` and `esp_wifi_deinit()` were being called. This destroyed the ESP32's hardware WiFi radio, causing the WiFi Scanner and Web Server to stop working afterward. These calls have been removed.
*   **Disabled NRF24 Modules:** In `config.h`, NRF24 modules 4 and 5 are disabled (`-1`) because the HSPI bus is dedicated to the two CC1101 modules in this configuration. The HSPI Test has been updated to test the CC1101s instead of the absent NRF24s.

### 4. UI & Menu Improvements
*   **Button Debounce:** A "wait for release" mechanism has been added to the main `loop()`. This prevents the device from immediately re-triggering a menu item if you are still holding the SELECT button while exiting a feature.
*   **Menu Alignment:** The `menuLabels` in `display.cpp` are now perfectly aligned with the `MenuItem` enum in `types.h`, eliminating empty spaces in the OLED menu.

## 🚀 Installation & Usage
1. Ensure you have the `SmartRC-CC1101-Driver-Lib` (version 2.5.7) installed in your Arduino IDE.
2. Edit `config.h` to add your home network SSID(s) and password(s).
3. Compile and upload the firmware.
4. Connect to the "Starbeam" network (if the fallback AP is active) or check your router's client list. Navigate to the IP address displayed on the OLED screen (e.g., `192.168.4.1`).

---
*Original Project by [dkyazzentwatwa](https://github.com/dkyazzentwatwa)*
