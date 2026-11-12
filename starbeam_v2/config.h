// config.h - Hardware Configuration for Project Starbeam V2
// 100% backward compatible with V1 hardware

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>


// In config.h
// ============================================================================
// WiFi Station (STA) Configuration
// ============================================================================
#define MAX_STA_NETWORKS 2
#define DEFAULT_WIFI_STA_ENABLED true  // Default to ONc:\Users\Rob\Downloads\starbeam\project-starbeam-main\project-starbeam-main\starbeam_v2\src\settings.h
#define DEFAULT_AP_ENABLED false       // Default AP to OFF

static const char* const STA_SSIDS[MAX_STA_NETWORKS] = {
    "wifi1",
    "wifi2"
};

static const char* const STA_PASSWORDS[MAX_STA_NETWORKS] = {
    "wifi1_pass",
    "wifi2_pass"
};


// ============================================================================
// NRF24L01 Radio Pin Definitions
// ============================================================================

// NRF24 Radio 1 (VSPI)
#define NRF24_1_CE  27
#define NRF24_1_CS  15

// NRF24 Radio 2 (VSPI)
#define NRF24_2_CE  26
#define NRF24_2_CS  33

// NRF24 Radio 3 (VSPI)
#define NRF24_3_CE  25
#define NRF24_3_CS  5

// // NRF24 Radio 4 (HSPI) - Can also be CC1101 #1
// #define NRF24_4_CE  4
// #define NRF24_4_CS  2

// // NRF24 Radio 5 (HSPI) - Can also be CC1101 #2
// #define NRF24_5_CE  32c:\Users\Rob\Downloads\starbeam\project-starbeam-main\project-starbeam-main\starbeam_v2\src\nrf24.cpp
// #define NRF24_5_CS  17

// NRF24 Radio 4 (HSPI) - 
#define NRF24_4_CE  -1
#define NRF24_4_CS  -1

// NRF24 Radio 5 (HSPI) - 
#define NRF24_5_CE  -1
#define NRF24_5_CS  -1
// ============================================================================
// CC1101 Radio Pin Definitions
// ============================================================================

// CC1101 #1 (HSPI)
#define CC1101_1_SCK   14
#define CC1101_1_MISO  12
#define CC1101_1_MOSI  13
#define CC1101_1_SS    2
#define CC1101_1_GDO0  4
#define CC1101_1_GDO2  16

// CC1101 #2 (HSPI)
#define CC1101_2_SCK   14
#define CC1101_2_MISO  12
#define CC1101_2_MOSI  13
#define CC1101_2_SS    32
#define CC1101_2_GDO0  35
#define CC1101_2_GDO2  17

// ============================================================================
// SPI Bus Definitions
// ============================================================================

// VSPI (for NRF24 radios 1-3)
#define VSPI_SCK   18
#define VSPI_MISO  19
#define VSPI_MOSI  23
#define VSPI_SS    5

// HSPI (for NRF24 radios 4-5, CC1101 radios)
#define HSPI_SCK   14
#define HSPI_MISO  12
#define HSPI_MOSI  13
#define HSPI_SS    2

// ============================================================================
// Display & UI Pin Definitions
// ============================================================================

// SSD1306 OLED Display (I2C)
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_I2C_ADDR 0x3C

// LED
#define LED_PIN -1
//#define LED_PIN 16

// Buttons
#define BUTTON_UP     39
#define BUTTON_DOWN   34
#define BUTTON_SELECT 36

// ============================================================================
// Buffer & Memory Sizes (EXACT MATCH to V1)
// ============================================================================

#define CCBUFFERSIZE         64     // CC1101 buffer size
#define RECORDINGBUFFERSIZE  4096   // Recording buffer size
#define EEPROM_SIZE          512    // EEPROM region reserved for raw recording (0..511)
#define EEPROM_TOTAL_SIZE    1024   // Total EEPROM partition (recording + settings + future)
#define SETTINGS_EEPROM_OFFSET 512  // Settings live at 512..575
#define SETTINGS_EEPROM_SIZE 64     // Reserved size for settings blob
#define BUF_LENGTH           128    // Command buffer length
#define MAX_SIGNALS          4      // Maximum signals to track

// ============================================================================
// Timing Constants (V2 Optimizations)
// ============================================================================

// Button handling
#define DEBOUNCE_MS      50      // Debounce time (v1 used blocking 100ms)
#define LONG_PRESS_MS    1000    // Long press detection threshold

// Display
#define DISPLAY_REFRESH_MS  50   // Display refresh interval (20 FPS max)

// Input polling
#define BUTTON_POLL_MS   10      // Button polling interval

// ============================================================================
// FreeRTOS Task Configuration
// ============================================================================

// Task stack sizes
#define TASK_STACK_SIZE_UI      4096
#define TASK_STACK_SIZE_INPUT   2048
#define TASK_STACK_SIZE_RADIO   8192
#define TASK_STACK_SIZE_OPS     8192

// Task priorities (0 = lowest, higher number = higher priority)
#define PRIORITY_UI       2
#define PRIORITY_INPUT    3  // Highest for responsiveness
#define PRIORITY_RADIO    2
#define PRIORITY_OPS      1

// Core assignment
#define CORE_RADIO  0  // Core 0 = Protocol CPU
#define CORE_UI     1  // Core 1 = Application CPU

// ============================================================================
// Radio Configuration
// ============================================================================

#define RF24_SPI_SPEED  16000000  // 16 MHz SPI speed for NRF24
#define RADIO_COUNT_NRF24  3      // Total NRF24 radios
#define RADIO_COUNT_CC1101 2      // Total CC1101 radios

// ============================================================================
// Web Server Configuration
// ============================================================================

#define WEB_SERVER_PORT      80
#define AP_SSID              "Starbeam"
#define AP_PASSWORD          "starbeam2024"
#define AP_CHANNEL           1
#define AP_HIDDEN            false
#define AP_MAX_CONNECTIONS   4

#endif // CONFIG_H
