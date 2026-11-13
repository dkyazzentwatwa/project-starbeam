// types.h - Type Definitions for Project Starbeam V2
// Shared enums, structs, and typedefs

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// ============================================================================
// Application State Machine (100% V1 compatible)
// ============================================================================

enum AppState {
  STATE_MENU,
  STATE_BT_JAM,
  STATE_DRONE_JAM,
  STATE_WIFI_JAM,
  STATE_CC1_JAM,
  STATE_CC_SCAN,
  STATE_NRF_SCAN,
  STATE_WIFI_SCAN,      // WiFi network scanner (uses ESP32 internal WiFi)
  STATE_WIFI_HEATMAP,   // WiFi channel heatmap (uses ESP32 internal WiFi)
  STATE_BLE_SCAN,       // BLE device scanner (uses ESP32 internal BLE)
  STATE_FLOCK_DETECTOR,   // Flock camera detector (promiscuous OUI scan)
  STATE_CAPTIVE_PORTAL,   // Captive portal (10 phishing templates, credential capture)
  STATE_PACKET_MONITOR,   // Passive WiFi packet monitor (promiscuous, per-frame stats)
  STATE_WEBSERVER,        // Web server with captive portal (independent mode)
  // Security Testing States
  STATE_SEC_DEAUTH_TARGET,    // Targeted deauth attack
  STATE_SEC_DEAUTH_ALL,       // Broadcast deauth attack
  STATE_SEC_BEACON_FLOOD,     // Beacon flooding
  STATE_SEC_PROBE_FLOOD,      // Probe request flooding
  STATE_SEC_PMKID_CAPTURE,    // PMKID capture
  STATE_CC1_SINGLE,
  STATE_CC2_SINGLE,
  STATE_REC_RAW,
  STATE_PLAY_RAW,
  STATE_SHOW_RAW,
  STATE_SHOW_BUFF,
  STATE_FLUSH_BUFF,
  STATE_GET_RSSI,
  STATE_STOP_ALL,
  STATE_RESET_CC,
  STATE_FREQ_PRESET,    // Unified CC1101 frequency preset (replaces 10 STATE_SET_*)
  STATE_SETTINGS,       // Settings view/edit
  STATE_TEST_NRF,
  STATE_TEST_NRF_5,
  STATE_TEST_CC1101
};

// ============================================================================
// Menu Items (100% V1 compatible)
// ============================================================================

enum MenuItem {
  BT_JAM,
  DRONE_JAM,
  WIFI_JAM,
  CC1_JAM,
  CC_SCAN,
  NRF_SCAN,
  WIFI_SCAN,      // WiFi network scanner
  WIFI_HEATMAP,   // WiFi channel heatmap
  BLE_SCAN,       // BLE device scanner
  FLOCK_DETECTOR,   // Flock camera detector
  CAPTIVE_PORTAL,   // Captive portal (10 templates)
  PACKET_MONITOR,   // Passive WiFi packet monitor
  WEBSERVER_ON,     // Start web server
  WEBSERVER_OFF,  // Stop web server
  WEBSERVER_STATUS, // Show web server status
  // Security Testing Menu Items
  SEC_DEAUTH_TARGET,    // Targeted deauth attack
  SEC_DEAUTH_ALL,       // Broadcast deauth attack
  SEC_BEACON_FLOOD,     // Beacon flooding
  SEC_PROBE_FLOOD,      // Probe request flooding
  SEC_PMKID_CAPTURE,    // PMKID capture
  TEST_NRF,
  TEST_CC1101,
  TEST_HSPI,
  WIFI_STA_MODE,  
  CC1_SINGLE,
  CC2_SINGLE,
  REC_RAW,
  PLAY_RAW,
  SHOW_RAW,
  SHOW_BUFF,
  GET_RSSI,
  FLUSH_BUFF,
  STOP_ALL,
  RESET_CC,
  SET_43440,
  SET_43430,
  SET_43400,
  SET_43390,
  SETTINGS,
  HELP,
  NUM_MENU_ITEMS
};

// ============================================================================
// Signal Information Structure (V1 compatible)
// ============================================================================

struct SignalInfo {
  float frequency;
  float rssi;
  unsigned long timestamp;
};

// ============================================================================
// V2-Specific Types
// ============================================================================

// Operation status for non-blocking state machines
enum OperationStatus {
  OP_IDLE,
  OP_RUNNING,
  OP_PAUSED,
  OP_COMPLETE,
  OP_ERROR
};

// Radio configuration structure
struct RadioConfig {
  uint8_t cePin;
  uint8_t csPin;
  uint8_t spiChannel;  // 0=VSPI, 1=HSPI
};

// Display update region for dirty tracking
struct DisplayRegion {
  uint16_t x, y, width, height;
  bool dirty;
};

// Button state for non-blocking debouncing
struct ButtonState {
  uint8_t pin;
  bool currentState;
  bool lastState;
  uint32_t lastChangeTime;
  bool longPressHandled;
};

// Button events for event queue
enum ButtonEvent {
  BTN_NONE,
  BTN_UP_PRESS,
  BTN_UP_LONG,
  BTN_DOWN_PRESS,
  BTN_DOWN_LONG,
  BTN_SELECT_PRESS,
  BTN_SELECT_LONG
};

// Radio command types for inter-task communication
enum RadioCommandType {
  RADIO_CMD_NRF24_TX,
  RADIO_CMD_NRF24_RX,
  RADIO_CMD_NRF24_SCAN,
  RADIO_CMD_CC1101_TX,
  RADIO_CMD_CC1101_RX,
  RADIO_CMD_CC1101_SCAN
};

// Radio command structure for queue-based communication
struct RadioCommand {
  RadioCommandType type;
  uint8_t radioId;
  uint8_t channel;
  uint32_t frequency;
  uint8_t data[64];
  uint8_t length;
  int16_t result;  // RSSI or status code
};

#endif // TYPES_H
