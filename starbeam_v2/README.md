# Project Starbeam V2 – Custom Fork

This repository contains a customized fork of the original [Project Starbeam V2](https://github.com/dkyazzentwatwa/project-starbeam) project.

This fork includes hardware compatibility fixes, improved WiFi connectivity, an automatically starting web server, terminal/CLI functionality, and various stability and user interface improvements.

> [!WARNING]
> **IMPORTANT: STARBEAM V2 ONLY**
>
> This firmware and all modifications described in this repository apply **exclusively to Starbeam V2 hardware**.
>
> It is NOT compatible with Starbeam V1.

> [!IMPORTANT]
> **HARDWARE CONFIGURATION**
>
> This firmware is optimized for the following hardware configuration:
>
> - **3× NRF24L01 modules** using VSPI.
> - **2× CC1101 modules** using HSPI.
>
> NRF24 radios 4 and 5 are disabled in `config.h` because the HSPI bus is dedicated to the two CC1101 modules.

> [!TIP]
> **DNF RESISTORS ARE NOT REQUIRED**
>
> Thanks to improved software-level SPI bus management, the DNF (Do Not Fit) resistors are no longer required for this configuration. They can remain unpopulated.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Hardware Configuration](#hardware-configuration)
- [Quick Start and Compilation](#quick-start-and-compilation)
  - [Prerequisites](#prerequisites)
  - [Required Libraries](#required-libraries)
  - [Compile and Upload Using Arduino CLI](#compile-and-upload-using-arduino-cli)
  - [Compile Using Arduino IDE](#compile-using-arduino-ide)
- [Changes and Improvements](#changes-and-improvements-in-this-fork)
  - [WiFi Station Mode and Automatic Web Server](#1-wifi-station-sta-mode-and-automatic-web-server)
  - [GPIO and Hardware Fixes](#2-hardware-and-gpio-fixes)
  - [NRF24 and WiFi Conflict Fix](#3-nrf24-and-wifi-conflict-fix)
  - [UI and Menu Improvements](#4-ui-and-menu-improvements)
- [Web Server and API](#web-server-and-api)
- [Terminal and CLI Mode](#terminal-and-cli-mode-headless-operation)
- [Configuration](#configuration)
- [Testing and Troubleshooting](#testing-and-troubleshooting)
- [License](#license)
- [Credits](#credits)

---

## Project Overview

Project Starbeam V2 is an ESP32-based multi-radio platform.

This custom fork focuses on improving hardware compatibility, reliability, connectivity, and remote accessibility.

### Main Features

- Automatic WiFi connection using up to three configured networks.
- Automatic fallback to WiFi Access Point mode.
- Automatically started web server.
- Web-based device monitoring and control.
- OLED display showing the active IP address.
- Serial terminal interface for headless operation.
- Improved GPIO compatibility.
- Improved SPI bus handling.
- NRF24 and CC1101 hardware configuration adjustments.
- Improved menu navigation and button handling.

---

## Hardware Configuration

This build is designed around the following radio configuration:

| Component | Quantity | SPI Bus | Status |
|---|---:|---|---|
| NRF24L01 | 3 | VSPI | Enabled |
| NRF24L01 | 2 (radio 4 and 5) | HSPI | Disabled |
| CC1101 | 2 | HSPI | Enabled |
| ESP32 | 1 | Internal | Enabled |

### SPI Bus Allocation

**VSPI**
- NRF24 module 1
- NRF24 module 2
- NRF24 module 3

**HSPI**
- CC1101 module 1
- CC1101 module 2

### DNF Resistors

The DNF resistors do not need to be populated for this hardware configuration.

The firmware handles the SPI bus configuration in software, eliminating the need for these resistors.

---

## Quick Start and Compilation

> [!IMPORTANT]
> **Required Partition Scheme: HUGE APP**
>
> This firmware requires the `huge_app` partition scheme to accommodate the program and its included features.
>
> The default partition scheme (approximately 1.2 MB application space) is insufficient.
>
> Use:
>
> `Huge APP (3MB No OTA/1MB SPIFFS)`

### Prerequisites

You will need:

- Arduino IDE 2.x or Arduino CLI.
- Compatible ESP32 development board.
- USB data cable.
- Required radio modules and peripherals.
- ESP32 board support package.

### ESP32 Board Support

Add the following URL to Arduino IDE under:

**File → Preferences → Additional Boards Manager URLs**

```text
https://dl.espressif.com/dl/package_esp32_index.json
```

Then install the ESP32 board package through Boards Manager.

### Required Libraries

Install the following libraries:

| Library | Required Version |
|---|---|
| Adafruit GFX Library | Latest compatible |
| Adafruit SSD1306 | Latest compatible |
| U8g2_for_Adafruit_GFX | Latest compatible |
| RF24 | Latest compatible |
| SmartRC-CC1101-Driver-Lib | **2.5.7** |

> [!IMPORTANT]
> The SmartRC-CC1101-Driver-Lib version is specifically pinned to **2.5.7** because this fork includes compatibility modifications for GPIO35.

### Compile and Upload Using Arduino CLI

Install the required libraries:

```bash
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "U8g2_for_Adafruit_GFX"
arduino-cli lib install "RF24"
arduino-cli lib install "SmartRC-CC1101-Driver-Lib@2.5.7"
```

Compile and upload:

```bash
arduino-cli compile --upload \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app \
  -p /dev/cu.SLAB_USBtoUART .
```

Replace `/dev/cu.SLAB_USBtoUART` with your actual ESP32 serial port.

Examples:

**Windows**
```bash
-p COM3
```

**Linux**
```bash
-p /dev/ttyUSB0
```

**macOS**
```bash
-p /dev/cu.SLAB_USBtoUART
```

### Compile Using Arduino IDE

1. Open `starbeam_v2.ino`.
2. Navigate to **Tools → Board → ESP32 Arduino**.
3. Select **ESP32 Dev Module**.
4. Navigate to **Tools → Partition Scheme**.
5. Select **Huge APP (3MB No OTA/1MB SPIFFS)**.
6. Navigate to **Tools → Port**.
7. Select your ESP32 serial port.
8. Click **Sketch → Upload**.

Make sure the correct partition scheme is selected before compiling.

---

## Changes and Improvements in This Fork

### 1. WiFi Station (STA) Mode and Automatic Web Server

The WiFi implementation has been improved to allow the device to connect automatically to existing wireless networks.

#### Automatic Startup

- The web server now starts automatically during `setup()`.
- No manual menu navigation is required to start the web interface.
- The device becomes accessible as soon as network initialization is complete.

#### Multiple WiFi Networks

The ESP32 can attempt to connect to up to three predefined WiFi networks during startup.

Networks are configured in `config.h`.

#### Automatic Access Point Fallback

If none of the configured WiFi networks are available, the ESP32 automatically switches to Access Point mode.

Default AP configuration:

| Setting | Value |
|---|---|
| SSID | `Starbeam` |
| Password | `starbeam2024` |
| Mode | Access Point |

This ensures that the device remains accessible even when no configured WiFi network is available.

#### WiFi Mode Menu Toggle

A new `WiFi Mode` menu option allows switching between:

- WiFi Station (STA) mode.
- WiFi Access Point (AP) mode.

#### OLED IP Address Display

The OLED display now shows the active IP address during startup.

This makes it easier to identify the address required to access the web interface.

---

### 2. Hardware and GPIO Fixes

Several ESP32 GPIO compatibility issues have been addressed.

#### GPIO35 – CC1101 Module 2 (GDO0)

The ESP32 has input-only GPIO pins.

GPIO34 through GPIO39 cannot be configured as output pins.

The original SmartRC CC1101 library configuration attempted to configure GDO0 as an output, causing initialization problems when using GPIO35.

**Modification:**

The `SmartRC_CC1101-Driver-Lib` has been patched so that GDO0 is no longer configured as an output.

This allows the second CC1101 module, connected to GPIO35, to initialize without triggering the GPIO output configuration error.

#### GPIO34 and GPIO39 – Button Inputs

GPIO34 and GPIO39 are input-only pins and do not have internal pull-up resistors.

The original implementation attempted to use:

```cpp
INPUT_PULLUP
```

This caused GPIO configuration errors, including:

```text
gpio_pullup_en(85)
```

**Modification:**

The `INPUT_PULLUP` configuration has been removed from the relevant button inputs in `analyzer.cpp`.

These pins now rely on the hardware's external pull-up resistors.

> [!NOTE]
> External pull-up resistors must be present for the affected inputs to operate correctly.

---

### 3. NRF24 and WiFi Conflict Fix

An important issue involving the ESP32's internal WiFi hardware was identified and corrected.

#### WiFi Hardware Preservation

In the original implementation, the following functions were called inside `analyzer.cpp`:

```cpp
esp_wifi_stop();
esp_wifi_deinit();
```

These calls stopped and deinitialized the ESP32 WiFi subsystem.

As a result, WiFi functionality could become unavailable after running certain analyzer operations.

This affected:

- WiFi scanning.
- Web server connectivity.
- Other WiFi-dependent functionality.

**Modification:**

The calls to `esp_wifi_stop()` and `esp_wifi_deinit()` have been removed.

The ESP32 WiFi subsystem remains initialized, allowing WiFi scanning and web server functionality to continue working.

#### Disabled NRF24 Modules 4 and 5

In `config.h`, NRF24 modules 4 and 5 have been disabled:

```cpp
-1
```

This is necessary because the HSPI bus is dedicated to the two CC1101 modules in this hardware configuration.

#### Updated HSPI Test

The HSPI hardware test has also been updated.

Instead of testing the disabled NRF24 modules, it now tests the two installed CC1101 modules.

---

### 4. UI and Menu Improvements

Several usability improvements have been made to the OLED menu system.

#### Button Debouncing and Release Detection

A button release detection mechanism has been added to the main `loop()`.

This prevents the device from immediately triggering another menu action when the SELECT button is still being held after exiting a feature.

Benefits:

- More reliable menu navigation.
- Reduced accidental selections.
- Improved button responsiveness.
- Fewer unintended repeated actions.

#### Menu Alignment

The `menuLabels` array in `display.cpp` has been aligned with the `MenuItem` enumeration in `types.h`.

This resolves menu alignment problems and eliminates empty entries caused by mismatched menu indexes.

---

## Web Server and API

The built-in web server provides remote access to Starbeam V2 functionality through a browser.

The web interface allows users to monitor device information and access available features without navigating the physical OLED menu.

### Available Endpoints

| Endpoint | Format | Description |
|---|---|---|
| `/api/wifi` | JSON | WiFi information |
| `/api/ble` | JSON | Bluetooth Low Energy information |
| `/api/attack/status` | JSON | Operation status |
| `/security` | HTML | Security interface |
| `/wifi` | HTML | WiFi interface |
| `/ble` | HTML | Bluetooth interface |

### Web Interface Features

Depending on the configured firmware features, the web interface provides:

- WiFi information and monitoring.
- BLE information and monitoring.
- Security testing controls.
- Operation status monitoring.
- Remote access to supported functions.

### Security Testing Interface

The web interface includes controls for the following security-testing operations:

- Deauthentication testing.
- Beacon flood testing.
- Probe flood testing.
- PMKID capture operations.

These features are intended for authorized testing on networks and devices that you own or have explicit permission to assess.

### Captive Portal

When operating in Access Point mode, the device includes captive portal functionality.

DNS requests are redirected to the main web interface, making it easier to access the Starbeam interface after connecting to the device's WiFi network.

---

## Terminal and CLI Mode (Headless Operation)

Starbeam V2 can also be controlled through its serial interface.

This makes it possible to operate the device without relying on the physical buttons or OLED display.

### Serial Configuration

| Setting | Value |
|---|---|
| Baud rate | 115200 |
| Connection | USB Serial |
| Interface | Serial Monitor / Terminal |

### Display Echo

The contents normally displayed on the OLED screen are also mirrored to the serial terminal.

This allows users to monitor the device's interface remotely through a serial connection.

### Available Commands

Type:

```text
help
```

The firmware displays the available command list, containing 36 commands.

Examples include:

```text
wifi_scan
bt_jam
deauth_target
```

The complete command list can be retrieved directly from the device using the `help` command.

### Stop Command

Any running operation can be interrupted using:

```text
stop
```

This provides a convenient way to stop active operations without using the physical controls.

---

## Configuration

Most hardware and connectivity settings can be changed in:

```text
config.h
```

### Hardware Pin Assignments

The configuration file contains the pin definitions for:

- NRF24 modules.
- CC1101 modules.
- OLED display.
- Physical buttons.
- SPI interfaces.

Make sure the configured pins match your actual Starbeam V2 hardware.

### WiFi Station Configuration

The firmware supports up to three predefined WiFi networks.

Locate the WiFi configuration in `config.h`:

```cpp
#define MAX_STA_NETWORKS 3

static const char* const STA_SSIDS[MAX_STA_NETWORKS] = {
    "YourHomeNetwork",
    "YourPhoneHotspot",
    "YourOfficeWiFi"
};

static const char* const STA_PASSWORDS[MAX_STA_NETWORKS] = {
    "password1",
    "password2",
    "password3"
};
```

Replace the example values with your own network credentials.

**Example:**

```cpp
#define MAX_STA_NETWORKS 3

static const char* const STA_SSIDS[MAX_STA_NETWORKS] = {
    "HomeWiFi",
    "MobileHotspot",
    "BackupNetwork"
};

static const char* const STA_PASSWORDS[MAX_STA_NETWORKS] = {
    "HomePassword",
    "MobilePassword",
    "BackupPassword"
};
```

> [!WARNING]
> Keep your WiFi credentials private. Do not commit personal network passwords to a public GitHub repository.

---

## Testing and Troubleshooting

### WiFi Scanner Returns 0 Networks

**Possible cause:**

The ESP32 WiFi subsystem has been stopped or deinitialized.

**Solution:**

Check `analyzer.cpp` and ensure the following calls are not being executed:

```cpp
esp_wifi_stop();
esp_wifi_deinit();
```

The WiFi subsystem must remain initialized for scanning and web server functionality to work correctly.

### Upload Fails

If uploading fails during the `Connecting...` stage:

1. Hold the ESP32 BOOT button.
2. Start the upload.
3. Release the BOOT button once the upload begins.
4. If necessary, reduce the upload speed.

A slower upload speed, such as:

```text
460800
```

may help with unreliable USB connections.

### Out of Memory / Firmware Too Large

**Possible cause:**

The default ESP32 partition scheme is being used.

**Solution:**

Select:

```text
Huge APP (3MB No OTA/1MB SPIFFS)
```

Make sure the same partition scheme is used when compiling through Arduino CLI.

### GPIO Errors During Startup

Check the following modifications:

- GPIO34 and GPIO39 must not use internal pull-ups.
- External pull-up resistors must be installed where required.
- GPIO35 must not be configured as an output.
- The patched SmartRC CC1101 library version 2.5.7 must be installed.

### CC1101 Module 2 Does Not Initialize

Check:

1. The CC1101 module is connected to the correct HSPI pins.
2. GDO0 is connected to GPIO35.
3. The patched SmartRC library is installed.
4. The correct pin assignments are defined in `config.h`.

### Web Interface Is Not Accessible

Check:

1. The ESP32 has completed startup.
2. The OLED displays a valid IP address.
3. Your computer or phone is connected to the same network when using STA mode.
4. You are connected to the `Starbeam` access point when using AP mode.
5. The web server starts successfully during `setup()`.

---

## License

This repository is a customized fork of the original Project Starbeam V2.

Original project:

[Project Starbeam – dkyazzentwatwa](https://github.com/dkyazzentwatwa/project-starbeam)

This fork maintains the original project's licensing terms.

Please refer to the [`LICENSE`](LICENSE) file for the applicable license and redistribution conditions.

---

## Credits

**Original Project**

[dkyazzentwatwa](https://github.com/dkyazzentwatwa)

**Custom Fork Maintainer**

[raspiuser1](https://github.com/raspiuser1)

Thanks to the original developer and the open-source community for making this project possible.

---

*Project Starbeam V2 – Custom Fork*

*Maintained by [raspiuser1](https://github.com/raspiuser1)*
