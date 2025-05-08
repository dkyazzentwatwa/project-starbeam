#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_event_loop.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <U8g2_for_Adafruit_GFX.h>

// BT
#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BleKeyboard.h>
#include <BLEServer.h>

// WIFI (uncomment DNS/WebServer if making a webserver)
#include <WiFi.h>
// #include <DNSServer.h>
// #include <WebServer.h>

// NRF24
#include "RF24.h"
#include <SPI.h>
#include "analyzer.h"
// CC1101
#include <ELECHOUSE_CC1101_SRC_DRV.h>
// 2nd CC1101
#include <ELECHOUSE_CC1101_SRC_DRV2.h>
#include <EEPROM.h>
// Create CC1101 instances
ELECHOUSE_CC1101 CC1;    // First radio
ELECHOUSE_CC1101_2 CC2;  // Second radio
#define CCBUFFERSIZE 64
#define RECORDINGBUFFERSIZE 4096  // Buffer for recording the frames
#define EPROMSIZE 512             // Size of EEPROM in your Arduino chip. For ESP32 it is Flash simulated only 512 bytes, ESP8266 is 4096
#define BUF_LENGTH 128            // Buffer for the incoming command.
#define MAX_SIGNALS 4             // Maximum number of signals to store and display

struct SignalInfo {
  float frequency;
  float rssi;
  unsigned long timestamp;
};

// NrF24 Triple Module Pin Setup
SPIClass vspi(VSPI);
SPIClass hspi(HSPI);
// radio(CE, CS)
RF24 radio(27, 15, 16000000);   // Radio 1
RF24 radio2(26, 33, 16000000);  // Radio 2
RF24 radio3(25, 5, 16000000);   // Radio 3

// RIGHT SWITCH, CC1101 has same pins as NRF24, but GDO02 is IRQ for NRF24 and not used
RF24 radio4(4, 2, 16000000);    // Radio 4 / CC1 on starbeam schematic
RF24 radio5(32, 17, 16000000);  // Radio 5 CC2

// CC1101 #1 -- CC1
const byte sck1 = 14;
const byte miso1 = 12;
const byte mosi1 = 13;
const byte ss1 = 2;
const int gdo0_1 = 4;
const int gdo2_1 = 16;

// CC1101 #2 - CC2
const byte sck2 = 14;
const byte miso2 = 12;
const byte mosi2 = 13;
const byte ss2 = 32;
const int gdo0_2 = 35;
const int gdo2_2 = 17;

// OLED display settings for SSD1306 128x64 .96inch -- change if using different display.
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SSD1306_I2C_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// U8g2 Fonts for Adafruit GFX
U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;

// LED
#define LED_PIN 16
// Buttons -- make sure to align with you setup
#define UP_BUTTON_PIN 39
#define DOWN_BUTTON_PIN 34
#define SELECT_BUTTON_PIN 36

// STATE MANAGEMENT -- This is where you control the logic of the menu options
enum AppState {
  STATE_MENU,
  STATE_BT_JAM,
  STATE_DRONE_JAM,
  STATE_WIFI_JAM,
  STATE_CC1_JAM,
  STATE_CC_SCAN,
  STATE_NRF_SCAN,
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
  STATE_SET_43392,
  STATE_SET_43400,
  STATE_SET_43390,
  STATE_SET_43387,
  STATE_SET_38800,
  STATE_SET_39000,
  STATE_SET_40000,
  STATE_SET_434500,
  STATE_SET_43440,
  STATE_SET_43430,
  STATE_TEST_NRF,
  STATE_TEST_NRF_5,
  STATE_TEST_CC1101,
};
// Global variable to keep track of the current state
AppState currentState = STATE_MENU;

// VARIABLES
enum MenuItem {
  BT_JAM,
  DRONE_JAM,
  WIFI_JAM,
  CC1_JAM,
  CC_SCAN,
  NRF_SCAN,
  TEST_NRF,
  TEST_CC1101,
  TEST_HSPI,
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

bool buttonPressed = false;
bool buttonEnabled = true;
uint32_t lastDrawTime;
uint32_t lastButtonTime;
int firstVisibleMenuItem = 0;
MenuItem selectedMenuItem = BT_JAM;

// Use this instead of delay()
void nonBlockingDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    // Allow ESP32 to handle background processes
    yield();  // Very important!
  }
}

// ------- GENERAL CONFIGURATION ------------

//Initialize the display -- make sure to change SSD1306 if using a different display
void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.display();
  delay(2000);
  display.clearDisplay();
}

//This is where the menu  is drawn
void drawMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  u8g2_for_adafruit_gfx.setFont(u8g2_font_baby_tf);  // Set back to small font

  // Title bar
  display.fillRect(0, 0, SCREEN_WIDTH, 16, SSD1306_WHITE);  // Top bar (16px)
  display.setTextColor(SSD1306_BLACK);                      // Black text on white bar
  display.setCursor(5, 4);                                  // Adjust as needed
  display.setTextSize(1);
  display.println("Home");  // Replace with dynamic title if needed

  // This is where you will add new menu items
  // Make sure to connect it with the enum MenuItem {... They must be in order
  const char *menuLabels[NUM_MENU_ITEMS] = {
    "BT Jammer", "Drone Jammer", "Wifi Jammer", "CC1 Jammer", "CC1101 Scan", "NRF Scan", "NRF Test", "CC1101 Test", "Test HSPI", "CC Single", "CC2 SINGLE", "Rec Raw", "Play Raw", "Show Raw", "Show Buffer", "Get RSSI 433mhz", "Flush Buffer", "Stop CC1101s", "Reset CC1101", "Set 434.440mhz", "Set 434.30mhz", "Set 434.400mhz", "Set 433.90mhz", "Settings",
    "Help"
  };
  // Menu items below title bar
  display.setTextColor(SSD1306_WHITE);  // White text in main menu area
  for (int i = 0; i < 2; i++) {         // Show 2 menu items at a time
    int menuIndex = (firstVisibleMenuItem + i) % NUM_MENU_ITEMS;
    int16_t x = 5;
    int16_t y = 20 + (i * 20);  // Adjust vertical spacing as needed

    // Highlight the selected item
    if (selectedMenuItem == menuIndex) {
      display.fillRect(0, y - 2, SCREEN_WIDTH, 15, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);  // Black text for highlighted item
    } else {
      display.setTextColor(SSD1306_WHITE);  // White text for non-highlighted items
    }
    display.setCursor(x, y);
    display.setTextSize(1);
    display.println(menuLabels[menuIndex]);
  }

  display.display();
}

// Draws the borders of the menu
void drawBorder() {
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
}

// DISPLAYS INFO ON THE SCREEN -- adjust to your display 
void displayInfo(String title, String info1 = "", String info2 = "", String info3 = "") {
  display.clearDisplay();
  drawBorder();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Title
  display.setCursor(4, 4);
  display.println(title);
  display.drawLine(0, 14, SCREEN_WIDTH, 14, SSD1306_WHITE);

  // Info lines
  display.setCursor(4, 18);
  display.println(info1);
  display.setCursor(4, 28);
  display.println(info2);
  display.setCursor(4, 38);
  display.println(info3);
  display.display();
}

void updateDisplay(const char *message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(message);
  display.display();
}

// Makes sure if a button is pressd
bool isButtonPressed(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    delay(100);  // Debounce delay
    if (digitalRead(pin) == LOW) {
      digitalWrite(LED_PIN, HIGH);  // Turn on LED
      return true;
    }
  }
  return false;
}

// This handles the menu selection
void handleMenuSelection() {
  static bool buttonPressed = false;

  if (!buttonPressed) {
    if (isButtonPressed(UP_BUTTON_PIN)) {
      // Wrap around if at the top
      selectedMenuItem = static_cast<MenuItem>((selectedMenuItem == 0) ? (NUM_MENU_ITEMS - 1) : (selectedMenuItem - 1));

      if (selectedMenuItem == (NUM_MENU_ITEMS - 1)) {
        // If wrapped to the bottom, make it visible
        firstVisibleMenuItem = NUM_MENU_ITEMS - 2;
      } else if (selectedMenuItem < firstVisibleMenuItem) {
        firstVisibleMenuItem = selectedMenuItem;
      }
      Serial.println("UP button pressed");
      drawMenu();
      buttonPressed = true;
    } else if (isButtonPressed(DOWN_BUTTON_PIN)) {
      // Wrap around if at the bottom
      selectedMenuItem = static_cast<MenuItem>((selectedMenuItem + 1) % NUM_MENU_ITEMS);

      if (selectedMenuItem == 0) {
        // If wrapped to the top, make it visible
        firstVisibleMenuItem = 0;
      } else if (selectedMenuItem >= (firstVisibleMenuItem + 2)) {
        firstVisibleMenuItem = selectedMenuItem - 1;
      }

      Serial.println("DOWN button pressed");
      drawMenu();
      buttonPressed = true;
    } else if (isButtonPressed(SELECT_BUTTON_PIN)) {
      Serial.println("SELECT button pressed");
      executeSelectedMenuItem();
      buttonPressed = true;
    }
  } else {
    // If no button is pressed, reset the buttonPressed flag
    if (!isButtonPressed(UP_BUTTON_PIN) && !isButtonPressed(DOWN_BUTTON_PIN) && !isButtonPressed(SELECT_BUTTON_PIN)) {
      buttonPressed = false;
      digitalWrite(LED_PIN, LOW);  // Turn off LED
    }
  }
}

// Intro Screen Graphics -- created from lopaka.app !!

static const unsigned char PROGMEM image_EviSmile1_bits[] = { 0x30, 0x03, 0x00, 0x60, 0x01, 0x80, 0xe0, 0x01, 0xc0, 0xf3, 0xf3, 0xc0, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xc0, 0x7f, 0xff, 0x80, 0x7f, 0xff, 0x80, 0x7f, 0xff, 0x80, 0xef, 0xfd, 0xc0, 0xe7, 0xf9, 0xc0, 0xe3, 0xf1, 0xc0, 0xe1, 0xe1, 0xc0, 0xf1, 0xe3, 0xc0, 0xff, 0xff, 0xc0, 0x7f, 0xff, 0x80, 0x7b, 0xf7, 0x80, 0x3d, 0x2f, 0x00, 0x1e, 0x1e, 0x00, 0x0f, 0xfc, 0x00, 0x03, 0xf0, 0x00 };
static const unsigned char PROGMEM image_Ble_connected_bits[] = { 0x07, 0xc0, 0x1f, 0xf0, 0x3e, 0xf8, 0x7e, 0x7c, 0x76, 0xbc, 0xfa, 0xde, 0xfc, 0xbe, 0xfe, 0x7e, 0xfc, 0xbe, 0xfa, 0xde, 0x76, 0xbc, 0x7e, 0x7c, 0x3e, 0xf8, 0x1f, 0xf0, 0x07, 0xc0 };
static const unsigned char PROGMEM image_MHz_bits[] = { 0xc3, 0x61, 0x80, 0x00, 0xe7, 0x61, 0x80, 0x00, 0xff, 0x61, 0x80, 0x00, 0xff, 0x61, 0xbf, 0x80, 0xdb, 0x7f, 0xbf, 0x80, 0xdb, 0x7f, 0x83, 0x00, 0xdb, 0x61, 0x86, 0x00, 0xc3, 0x61, 0x8c, 0x00, 0xc3, 0x61, 0x98, 0x00, 0xc3, 0x61, 0xbf, 0x80, 0xc3, 0x61, 0xbf, 0x80 };
static const unsigned char PROGMEM image_Error_bits[] = { 0x03, 0xf0, 0x00, 0x0f, 0xfc, 0x00, 0x1f, 0xfe, 0x00, 0x3f, 0xff, 0x00, 0x73, 0xf3, 0x80, 0x71, 0xe3, 0x80, 0xf8, 0xc7, 0xc0, 0xfc, 0x0f, 0xc0, 0xfe, 0x1f, 0xc0, 0xfe, 0x1f, 0xc0, 0xfc, 0x0f, 0xc0, 0xf8, 0xc7, 0xc0, 0x71, 0xe3, 0x80, 0x73, 0xf3, 0x80, 0x3f, 0xff, 0x00, 0x1f, 0xfe, 0x00, 0x0f, 0xfc, 0x00, 0x03, 0xf0, 0x00 };
static const unsigned char PROGMEM image_Bluetooth_Idle_bits[] = { 0x20, 0xb0, 0x68, 0x30, 0x30, 0x68, 0xb0, 0x20 };
static const unsigned char PROGMEM image_off_text_bits[] = { 0x67, 0x70, 0x94, 0x40, 0x96, 0x60, 0x94, 0x40, 0x64, 0x40 };
static const unsigned char PROGMEM image_wifi_not_connected_bits[] = { 0x21, 0xf0, 0x00, 0x16, 0x0c, 0x00, 0x08, 0x03, 0x00, 0x25, 0xf0, 0x80, 0x42, 0x0c, 0x40, 0x89, 0x02, 0x20, 0x10, 0xa1, 0x00, 0x23, 0x58, 0x80, 0x04, 0x24, 0x00, 0x08, 0x52, 0x00, 0x01, 0xa8, 0x00, 0x02, 0x04, 0x00, 0x00, 0x42, 0x00, 0x00, 0xa1, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00, 0x00 };
static const unsigned char PROGMEM image_volume_muted_bits[] = { 0x01, 0xc0, 0x00, 0x02, 0x40, 0x00, 0x04, 0x40, 0x00, 0x08, 0x40, 0x00, 0xf0, 0x50, 0x40, 0x80, 0x48, 0x80, 0x80, 0x45, 0x00, 0x80, 0x42, 0x00, 0x80, 0x45, 0x00, 0x80, 0x48, 0x80, 0xf0, 0x50, 0x40, 0x08, 0x40, 0x00, 0x04, 0x40, 0x00, 0x02, 0x40, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x00 };
static const unsigned char PROGMEM image_network_not_connected_bits[] = { 0x82, 0x0e, 0x44, 0x0a, 0x28, 0x0a, 0x10, 0x0a, 0x28, 0xea, 0x44, 0xaa, 0x82, 0xaa, 0x00, 0xaa, 0x0e, 0xaa, 0x0a, 0xaa, 0x0a, 0xaa, 0x0a, 0xaa, 0xea, 0xaa, 0xaa, 0xaa, 0xee, 0xee, 0x00, 0x00 };
static const unsigned char PROGMEM image_microphone_muted_bits[] = { 0x87, 0x00, 0x4f, 0x80, 0x26, 0x80, 0x13, 0x80, 0x09, 0x80, 0x04, 0x80, 0x0a, 0x00, 0x0d, 0x00, 0x2e, 0xa0, 0x27, 0x40, 0x10, 0x20, 0x0f, 0x90, 0x02, 0x08, 0x02, 0x04, 0x0f, 0x82, 0x00, 0x00 };
static const unsigned char PROGMEM image_mute_text_bits[] = { 0x8a, 0x5d, 0xe0, 0xda, 0x49, 0x00, 0xaa, 0x49, 0xc0, 0x8a, 0x49, 0x00, 0x89, 0x89, 0xe0 };
static const unsigned char PROGMEM image_cross_contour_bits[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x80, 0x51, 0x40, 0x8a, 0x20, 0x44, 0x40, 0x20, 0x80, 0x11, 0x00, 0x20, 0x80, 0x44, 0x40, 0x8a, 0x20, 0x51, 0x40, 0x20, 0x80, 0x00, 0x00, 0x00, 0x00 };

// Intro Screen
void demonSHIT() {
  display.clearDisplay();
  u8g2_for_adafruit_gfx.setFont(u8g2_font_adventurer_tr);  // Use a larger font for the title
  u8g2_for_adafruit_gfx.setCursor(20, 40);                 // Centered vertically
  display.drawBitmap(56, 40, image_EviSmile1_bits, 18, 21, 1);
  display.setTextWrap(false);
  u8g2_for_adafruit_gfx.setCursor(30, 18);
  u8g2_for_adafruit_gfx.print("PROJECT");
  u8g2_for_adafruit_gfx.setCursor(28, 35);
  u8g2_for_adafruit_gfx.print("STARBEAM");
  display.drawBitmap(106, 19, image_Ble_connected_bits, 15, 15, 1);
  display.drawBitmap(2, 50, image_MHz_bits, 25, 11, 1);
  display.drawBitmap(1, 1, image_Error_bits, 18, 18, 1);
  display.drawBitmap(25, 38, image_Bluetooth_Idle_bits, 5, 8, 1);
  display.drawBitmap(83, 55, image_off_text_bits, 12, 5, 1);
  display.drawBitmap(109, 2, image_wifi_not_connected_bits, 19, 16, 1);
  display.drawBitmap(4, 31, image_volume_muted_bits, 18, 16, 1);
  display.drawBitmap(109, 45, image_network_not_connected_bits, 15, 16, 1);
  display.drawBitmap(92, 33, image_microphone_muted_bits, 15, 16, 1);
  display.drawBitmap(1, 23, image_mute_text_bits, 19, 5, 1);
  display.drawBitmap(32, 49, image_cross_contour_bits, 11, 16, 1);
  display.display();
}

// Displays the title screen  
void displayTitleScreen() {
  display.clearDisplay();
  u8g2_for_adafruit_gfx.setFont(u8g2_font_adventurer_tr);  // Use a larger font for the title
  u8g2_for_adafruit_gfx.setCursor(20, 40);                 // Centered vertically
  u8g2_for_adafruit_gfx.print("CYPHER BOX");
  // u8g2_for_adafruit_gfx.setCursor(centerX, 25); // Centered vertically
  // u8g2_for_adafruit_gfx.print("NETWORK PET");
  display.display();
}
void displayInfoScreen() {
  display.clearDisplay();
  u8g2_for_adafruit_gfx.setFont(u8g2_font_baby_tf);  // Set back to small font
  u8g2_for_adafruit_gfx.setCursor(0, 22);
  u8g2_for_adafruit_gfx.print("Welcome to PROJECT STARBEAM!");

  u8g2_for_adafruit_gfx.setCursor(0, 30);
  u8g2_for_adafruit_gfx.print("This is a cool cyber tool.");

  u8g2_for_adafruit_gfx.setCursor(0, 38);
  u8g2_for_adafruit_gfx.print("I perform analysis & attacks.");

  u8g2_for_adafruit_gfx.setCursor(0, 46);
  u8g2_for_adafruit_gfx.print("Add radios to have some fun!!");

  u8g2_for_adafruit_gfx.setCursor(0, 54);
  u8g2_for_adafruit_gfx.print("Have fun & be safe ~_~;");

  display.display();
}
// ------- GENERAL CONFIGURATION END ------------

//// ------- NRF24 SETUP ------------

// Top 3 Radios of StarBeam
void initRadios() {
  display.clearDisplay();
  u8g2_for_adafruit_gfx.setFont(u8g2_font_baby_tf);

  // Ensure proper SPI pin configuration for VSPI
  vspi.begin();
  pinMode(33, INPUT_PULLUP);  // Soft pull-up
  pinMode(26, OUTPUT);        // Radio 2 CE

  // Display status
  u8g2_for_adafruit_gfx.setCursor(0, 10);
  u8g2_for_adafruit_gfx.print("Initializing Radios...");
  display.display();
  delay(500);

  // Function to initialize radios
  bool radio1_status = initializeRadio(radio, "Radio 1", 22);
  nonBlockingDelay(2000);  // Small delay between radio initializations
  bool radio2_status = initializeRadio(radio2, "Radio 2", 30);
  nonBlockingDelay(2000);
  bool radio3_status = initializeRadio(radio3, "Radio 3", 38);

  // Final display update
  display.display();
}


// Helper function to initialize each radio
bool initializeRadio(RF24 &radio, const char *name, int yPos) {
  u8g2_for_adafruit_gfx.setCursor(0, yPos);

  if (radio.begin(&vspi)) {
    Serial.printf("%s Started\n", name);
    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.printPrettyDetails();
    radio.startConstCarrier(RF24_PA_MAX, 45);

    u8g2_for_adafruit_gfx.print(name);
    u8g2_for_adafruit_gfx.print(" Initialized!");
    return true;
  } else {
    Serial.printf("%s Failed to Start\n", name);
    u8g2_for_adafruit_gfx.print(name);
    u8g2_for_adafruit_gfx.print(" Failed!");
    return false;
  }
}

// Testing 2 NRF's #4(left radio) & #5 (right radio) on HSPI
// Even if the radios arent connected, it wont mess up operation
void initRadiosHspi() {
  display.clearDisplay();
  u8g2_for_adafruit_gfx.setFont(u8g2_font_baby_tf);

  // Ensure proper SPI pin configuration for HSPI
  hspi.begin();
  // pinMode(33, INPUT_PULLUP);  // Soft pull-up
  // pinMode(26, OUTPUT);        // Radio 2 CE
  //  Initialize each radio
  u8g2_for_adafruit_gfx.setCursor(0, 40);
  u8g2_for_adafruit_gfx.print("Initializing Radios...");
  display.display();
  delay(500);  // Small delay for readability
  u8g2_for_adafruit_gfx.setCursor(0, 45);
  if (radio4.begin(&hspi)) {
    Serial.println("Radio 4 Started");
    radio4.setAutoAck(false);
    radio4.stopListening();
    radio4.setRetries(0, 0);
    radio4.setPALevel(RF24_PA_MAX, true);
    radio4.setDataRate(RF24_2MBPS);
    radio4.setCRCLength(RF24_CRC_DISABLED);
    radio4.printPrettyDetails();
    radio4.startConstCarrier(RF24_PA_MAX, 45);
    u8g2_for_adafruit_gfx.print("Radio 4 Initialized!");
  } else {
    Serial.println("Radio 4 Failed to Start");
    u8g2_for_adafruit_gfx.print("Radio 4 Failed!");
  }
  u8g2_for_adafruit_gfx.setCursor(0, 53);
  if (radio5.begin(&hspi)) {
    Serial.println("Radio 5 Started");
    radio5.setAutoAck(false);
    radio5.stopListening();
    radio5.setRetries(0, 0);
    radio5.setPALevel(RF24_PA_MAX, true);
    radio5.setDataRate(RF24_2MBPS);
    radio5.setCRCLength(RF24_CRC_DISABLED);
    radio5.printPrettyDetails();
    radio5.startConstCarrier(RF24_PA_MAX, 45);
    u8g2_for_adafruit_gfx.print("Radio 5 Initialized!");
  } else {
    Serial.println("Radio 5 Failed to Start");
    u8g2_for_adafruit_gfx.print("Radio 5 Failed!");
  }
  display.display();
}

/* ---- NRF24 JAMMER FUNCTIONS ----
---
Various options to use the 2.4ghz jammer to give you some ideas:
btJam - for jamming bluetooth devices
droneJam - for jamming drone devices at 2.4ghz
one() - all radios on same random channel range
singleChannel() - select individual channel range for each radio
multipleChannels() - bounce between specific channels for focused tests
channelRange() - provide a specific range to transmit, i.e. bluetooth 40-80, wifi 1-14, etc
Test around & see what works best!
---
*/
void btJam() {
  ////RANDOM CHANNEL
  radio2.setChannel(random(81));
  radio3.setChannel(random(81));
  radio.setChannel(random(81));
  radio4.setChannel(random(81));
  radio5.setChannel(random(81));
  delayMicroseconds(random(60));
  /*  YOU CAN DO -----SWEEP CHANNEL
  for (int i = 0; i < 79; i++) {
    radio.setChannel(i);
*/
}

void droneJam() {
  ////RANDOM CHANNEL
  radio2.setChannel(random(126));
  radio3.setChannel(random(126));
  radio.setChannel(random(126));
  radio4.setChannel(random(126));
  radio5.setChannel(random(126));
  delayMicroseconds(random(60));  //////REMOVE IF SLOW
  /*  YOU CAN DO -----SWEEP CHANNEL
  for (int i = 0; i < 79; i++) {
    radio.setChannel(i);
*/
}

void singleChannel() {
  ////RANDOM CHANNEL
  radio2.setChannel(random(81));
  radio3.setChannel(random(15));
  radio.setChannel(random(15));
  radio4.setChannel(random(81));
  radio5.setChannel(random(81));

  delayMicroseconds(random(60));  //////REMOVE IF SLOW
}
void wifiJam() {
  // Define the set of channels you want to choose from
  int numbers[] = { 1, 6, 14 };
  int sizeOfArray = sizeof(numbers) / sizeof(numbers[0]);  // Calculate the size of the array

  // Generate a random index
  int randomIndex = random(sizeOfArray);  // random(max) generates a number from 0 to max-1

  // Select the random number from the array
  int randomNumber = numbers[randomIndex];

  radio.setChannel(randomNumber);
  radio2.setChannel(randomNumber);
  radio3.setChannel(randomNumber);
  radio4.setChannel(randomNumber);
  radio5.setChannel(randomNumber);


  // Output the result to the Serial Monitor
  Serial.print("Randomly selected channel: ");
  Serial.println(randomNumber);
}

void channelRange() {
  int randomNumber = random(40, 81);  // 81 because the upper bound is exclusive

  // Output the result to the Serial Monitor
  Serial.print("Randomly selected number between 40 and 80: ");
  Serial.println(randomNumber);

  // Example usage with radio.setChannel
  radio.setChannel(randomNumber);
  radio2.setChannel(randomNumber);
  radio3.setChannel(randomNumber);
  radio4.setChannel(randomNumber);
  radio5.setChannel(randomNumber);
}
// ------- NRF24 SETUP END ------------

// ------- CC1101 SETUP START ------------
// position in big recording buffer
int bigrecordingbufferpos = 0;

// number of frames in big recording buffer
int framesinbigrecordingbuffer = 0;

// check if CLI receiving mode enabled
int receivingmode = 0;

// check if CLI jamming mode enabled
int jammingmode = 0;

// check if CLI recording mode enabled
int recordingmode = 0;

// check if CLI chat mode enabled
int chatmode = 0;

static bool do_echo = true;

// buffer for receiving  CC1101
byte ccreceivingbuffer[CCBUFFERSIZE] = { 0 };

// buffer for sending  CC1101
byte ccsendingbuffer[CCBUFFERSIZE] = { 0 };
// char ccsendingbuffer[CCBUFFERSIZE] = {0};

// buffer for recording and replaying of many frames
byte bigrecordingbuffer[RECORDINGBUFFERSIZE] = { 0 };

// buffer for hex to ascii conversions
byte textbuffer[BUF_LENGTH];
// char textbuffer[BUF_LENGTH];

// convert bytes in table to string with hex numbers
void asciitohex(byte *ascii_ptr, byte *hex_ptr, int len) {
  byte i, j, k;
  for (i = 0; i < len; i++) {
    // high byte first
    j = ascii_ptr[i] / 16;
    if (j > 9) {
      k = j - 10 + 65;
    } else {
      k = j + 48;
    }
    hex_ptr[2 * i] = k;
    // low byte second
    j = ascii_ptr[i] % 16;
    if (j > 9) {
      k = j - 10 + 65;
    } else {
      k = j + 48;
    }
    hex_ptr[(2 * i) + 1] = k;
  };
  hex_ptr[(2 * i) + 2] = '\0';
}

// convert string with hex numbers to array of bytes
void hextoascii(byte *ascii_ptr, byte *hex_ptr, int len) {
  byte i, j;
  for (i = 0; i < (len / 2); i++) {
    j = hex_ptr[i * 2];
    if ((j > 47) && (j < 58))
      ascii_ptr[i] = (j - 48) * 16;
    if ((j > 64) && (j < 71))
      ascii_ptr[i] = (j - 55) * 16;
    if ((j > 96) && (j < 103))
      ascii_ptr[i] = (j - 87) * 16;
    j = hex_ptr[i * 2 + 1];
    if ((j > 47) && (j < 58))
      ascii_ptr[i] = ascii_ptr[i] + (j - 48);
    if ((j > 64) && (j < 71))
      ascii_ptr[i] = ascii_ptr[i] + (j - 55);
    if ((j > 96) && (j < 103))
      ascii_ptr[i] = ascii_ptr[i] + (j - 87);
  };
  ascii_ptr[i++] = '\0';
}

/*
FIX FOR BAD GDO0
CHANGE in CC1_SRC_DRV.cpp
void CC1::GDO_Set (void)

{
  pinMode(GDO0, OUTPUT);
  pinMode(GDO2, INPUT);
}
*/
// Initialize CC1101 board with default settings, you may change your preferences here
static void cc1101initialize(void) {
  // initializing library with custom pins selected
  CC1.setSpiPin(sck1, miso1, mosi1, ss1);
  CC1.setGDO(gdo0_1, gdo2_1);
  // Main part to tune CC1101 with proper frequency, modulation and encoding
  CC1.Init();  // must be set to initialize the cc1101!
  CC1.setGDO0(gdo0_1);
  CC1.setCCMode(1);           // set config for internal transmission mode. value 0 is for RAW recording/replaying
  CC1.setModulation(2);       // set modulation mode. 0 = 2-FSK, 1 = GFSK, 2 = ASK/OOK, 3 = 4-FSK, 4 = MSK.
  CC1.setMHZ(433.92);         // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.
  CC1.setDeviation(47.60);    // Set the Frequency deviation in kHz. Value from 1.58 to 380.85. Default is 47.60 kHz.
  CC1.setChannel(0);          // Set the Channelnumber from 0 to 255. Default is cahnnel 0.
  CC1.setChsp(199.95);        // The channel spacing is multiplied by the channel number CHAN and added to the base frequency in kHz. Value from 25.39 to 405.45. Default is 199.95 kHz.
  CC1.setRxBW(812.50);        // Set the Receive Bandwidth in kHz. Value from 58.03 to 812.50. Default is 812.50 kHz.
  CC1.setDRate(9.6);          // Set the Data Rate in kBaud. Value from 0.02 to 1621.83. Default is 99.97 kBaud!
  CC1.setPA(10);              // Set TxPower. The following settings are possible depending on the frequency band.  (-30  -20  -15  -10  -6    0    5    7    10   11   12) Default is max!
  CC1.setSyncMode(2);         // Combined sync-word qualifier mode. 0 = No preamble/sync. 1 = 16 sync word bits detected. 2 = 16/16 sync word bits detected. 3 = 30/32 sync word bits detected. 4 = No preamble/sync, carrier-sense above threshold. 5 = 15/16 + carrier-sense above threshold. 6 = 16/16 + carrier-sense above threshold. 7 = 30/32 + carrier-sense above threshold.
  CC1.setSyncWord(211, 145);  // Set sync word. Must be the same for the transmitter and receiver. Default is 211,145 (Syncword high, Syncword low)
  CC1.setAdrChk(0);           // Controls address check configuration of received packages. 0 = No address check. 1 = Address check, no broadcast. 2 = Address check and 0 (0x00) broadcast. 3 = Address check and 0 (0x00) and 255 (0xFF) broadcast.
  CC1.setAddr(0);             // Address used for packet filtration. Optional broadcast addresses are 0 (0x00) and 255 (0xFF).
  CC1.setWhiteData(0);        // Turn data whitening on / off. 0 = Whitening off. 1 = Whitening on.
  CC1.setPktFormat(0);        // Format of RX and TX data. 0 = Normal mode, use FIFOs for RX and TX. 1 = Synchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins. 2 = Random TX mode; sends random data using PN9 generator. Used for test. Works as normal mode, setting 0 (00), in RX. 3 = Asynchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins.
  CC1.setLengthConfig(1);     // 0 = Fixed packet length mode. 1 = Variable packet length mode. 2 = Infinite packet length mode. 3 = Reserved
  CC1.setPacketLength(0);     // Indicates the packet length when fixed packet length mode is enabled. If variable packet length mode is used, this value indicates the maximum packet length allowed.
  CC1.setCrc(0);              // 1 = CRC calculation in TX and CRC check in RX enabled. 0 = CRC disabled for TX and RX.
  CC1.setCRC_AF(0);           // Enable automatic flush of RX FIFO when CRC is not OK. This requires that only one packet is in the RXIFIFO and that packet length is limited to the RX FIFO size.
  CC1.setDcFilterOff(0);      // Disable digital DC blocking filter before demodulator. Only for data rates ≤ 250 kBaud The recommended IF frequency changes when the DC blocking is disabled. 1 = Disable (current optimized). 0 = Enable (better sensitivity).
  CC1.setManchester(0);       // Enables Manchester encoding/decoding. 0 = Disable. 1 = Enable.
  CC1.setFEC(0);              // Enable Forward Error Correction (FEC) with interleaving for packet payload (Only supported for fixed packet length mode. 0 = Disable. 1 = Enable.
  CC1.setPRE(0);              // Sets the minimum number of preamble bytes to be transmitted. Values: 0 : 2, 1 : 3, 2 : 4, 3 : 6, 4 : 8, 5 : 12, 6 : 16, 7 : 24
  CC1.setPQT(0);              // Preamble quality estimator threshold. The preamble quality estimator increases an internal counter by one each time a bit is received that is different from the previous bit, and decreases the counter by 8 each time a bit is received that is the same as the last bit. A threshold of 4∙PQT for this counter is used to gate sync word detection. When PQT=0 a sync word is always accepted.
  CC1.setAppendStatus(0);     // When enabled, two status bytes will be appended to the payload of the packet. The status bytes contain RSSI and LQI values, as well as CRC OK.
}

// CC1101 inititialization using 2nd library
static void cc1101initialize_2(void) {
  // initializing library with custom pins selected
  CC2.setSpiPin(sck2, miso2, mosi2, ss2);
  CC2.setGDO(gdo0_2, gdo2_2);
  // Main part to tune CC1101 with proper frequency, modulation and encoding
  CC2.Init();  // must be set to initialize the cc1101!
  CC2.setGDO0(gdo0_2);
  CC2.setCCMode(1);           // set config for internal transmission mode. value 0 is for RAW recording/replaying
  CC2.setModulation(2);       // set modulation mode. 0 = 2-FSK, 1 = GFSK, 2 = ASK/OOK, 3 = 4-FSK, 4 = MSK.
  CC2.setMHZ(433.92);         // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.
  CC2.setDeviation(47.60);    // Set the Frequency deviation in kHz. Value from 1.58 to 380.85. Default is 47.60 kHz.
  CC2.setChannel(0);          // Set the Channelnumber from 0 to 255. Default is cahnnel 0.
  CC2.setChsp(199.95);        // The channel spacing is multiplied by the channel number CHAN and added to the base frequency in kHz. Value from 25.39 to 405.45. Default is 199.95 kHz.
  CC2.setRxBW(812.50);        // Set the Receive Bandwidth in kHz. Value from 58.03 to 812.50. Default is 812.50 kHz.
  CC2.setDRate(9.6);          // Set the Data Rate in kBaud. Value from 0.02 to 1621.83. Default is 99.97 kBaud!
  CC2.setPA(10);              // Set TxPower. The following settings are possible depending on the frequency band.  (-30  -20  -15  -10  -6    0    5    7    10   11   12) Default is max!
  CC2.setSyncMode(2);         // Combined sync-word qualifier mode. 0 = No preamble/sync. 1 = 16 sync word bits detected. 2 = 16/16 sync word bits detected. 3 = 30/32 sync word bits detected. 4 = No preamble/sync, carrier-sense above threshold. 5 = 15/16 + carrier-sense above threshold. 6 = 16/16 + carrier-sense above threshold. 7 = 30/32 + carrier-sense above threshold.
  CC2.setSyncWord(211, 145);  // Set sync word. Must be the same for the transmitter and receiver. Default is 211,145 (Syncword high, Syncword low)
  CC2.setAdrChk(0);           // Controls address check configuration of received packages. 0 = No address check. 1 = Address check, no broadcast. 2 = Address check and 0 (0x00) broadcast. 3 = Address check and 0 (0x00) and 255 (0xFF) broadcast.
  CC2.setAddr(0);             // Address used for packet filtration. Optional broadcast addresses are 0 (0x00) and 255 (0xFF).
  CC2.setWhiteData(0);        // Turn data whitening on / off. 0 = Whitening off. 1 = Whitening on.
  CC2.setPktFormat(0);        // Format of RX and TX data. 0 = Normal mode, use FIFOs for RX and TX. 1 = Synchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins. 2 = Random TX mode; sends random data using PN9 generator. Used for test. Works as normal mode, setting 0 (00), in RX. 3 = Asynchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins.
  CC2.setLengthConfig(1);     // 0 = Fixed packet length mode. 1 = Variable packet length mode. 2 = Infinite packet length mode. 3 = Reserved
  CC2.setPacketLength(0);     // Indicates the packet length when fixed packet length mode is enabled. If variable packet length mode is used, this value indicates the maximum packet length allowed.
  CC2.setCrc(0);              // 1 = CRC calculation in TX and CRC check in RX enabled. 0 = CRC disabled for TX and RX.
  CC2.setCRC_AF(0);           // Enable automatic flush of RX FIFO when CRC is not OK. This requires that only one packet is in the RXIFIFO and that packet length is limited to the RX FIFO size.
  CC2.setDcFilterOff(0);      // Disable digital DC blocking filter before demodulator. Only for data rates ≤ 250 kBaud The recommended IF frequency changes when the DC blocking is disabled. 1 = Disable (current optimized). 0 = Enable (better sensitivity).
  CC2.setManchester(0);       // Enables Manchester encoding/decoding. 0 = Disable. 1 = Enable.
  CC2.setFEC(0);              // Enable Forward Error Correction (FEC) with interleaving for packet payload (Only supported for fixed packet length mode. 0 = Disable. 1 = Enable.
  CC2.setPRE(0);              // Sets the minimum number of preamble bytes to be transmitted. Values: 0 : 2, 1 : 3, 2 : 4, 3 : 6, 4 : 8, 5 : 12, 6 : 16, 7 : 24
  CC2.setPQT(0);              // Preamble quality estimator threshold. The preamble quality estimator increases an internal counter by one each time a bit is received that is different from the previous bit, and decreases the counter by 8 each time a bit is received that is the same as the last bit. A threshold of 4∙PQT for this counter is used to gate sync word detection. When PQT=0 a sync word is always accepted.
  CC2.setAppendStatus(0);     // When enabled, two status bytes will be appended to the payload of the packet. The status bytes contain RSSI and LQI values, as well as CRC OK.
}

// Function Declarations
void printHelp();
void setModulation(int setting);
void setMhz(float settingf1);
void setDeviation(float settingf1);
void setChannel(int setting);
void setChsp(float settingf1);
void setRxBw(float settingf1);
void setDRate(float settingf1);
void setPa(int setting);
void setSyncMode(int setting);
void setSyncWord(int setting, int setting2);
void setAdrChk(int setting);
void setAddr(int setting);
void setWhiteData(int setting);
void setPktFormat(int setting);
void setLengthConfig(int setting);
void setPacketLength(int setting);
void setCrc(int setting);
void setCrcAf(int setting);
void setDcFilterOff(int setting);
void setManchester(int setting);
void setFec(int setting);
void setPre(int setting);
void setPqt(int setting);
void setAppendStatus(int setting);
void getRssi();
void scan(float settingf1, float settingf2);
void save();
void load();
void toggleRxMode();
void toggleChatMode();
void toggleJammingMode();
void bruteForce(int setting, int setting2);
void transmitData(char *cmdline);
void recordRawData(int setting);
void sniffRawData(int setting);
void playRawData(int setting);
void showRawData();
void showBitData();
void addRawData(char *cmdline);
void toggleRecordingMode();
void playRecordedFrames(int setting);
void addFrame(char *cmdline);
void showRecordedFrames();
void flushRecordingBuffer();
void setEchoMode(int do_echo);
void stopAllModes();
void initializeCC1101();

// Function Definitions

void printHelp() {
  Serial.println(F(
    "setmodulation <mode> : Set modulation mode. 0 = 2-FSK, 1 = GFSK, 2 = ASK/OOK, 3 = 4-FSK, 4 = MSK.\r\n\r\n"
    "setmhz <frequency>   : Here you can set your basic frequency. default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ.\r\n\r\n"
    "setdeviation <deviation> : Set the Frequency deviation in kHz. Value from 1.58 to 380.85.\r\n\r\n"
    "setchannel <channel> : Set the Channelnumber from 0 to 255. Default is cahnnel 0.\r\n\r\n"
    "setchsp <spacing>  :  The channel spacing is multiplied by the channel number CHAN and added to the base frequency in kHz. Value from 25.39 to 405.45. \r\n\r\n"
    "setrxbw <Receive bndwth> : Set the Receive Bandwidth in kHz. Value from 58.03 to 812.50. \r\n\r\n"
    "setdrate <datarate> : Set the Data Rate in kBaud. Value from 0.02 to 1621.83.\r\n\r\n"
    "setpa <power value> : Set RF transmission power. The following settings are possible depending on the frequency band.  (-30  -20  -15  -10  -6    0    5    7    10   11   12) Default is max!\r\n\r\n"
    "setsyncmode  <sync mode> : Combined sync-word qualifier mode. 0 = No preamble/sync. 1 = 16 sync word bits detected. 2 = 16/16 sync word bits detected. 3 = 30/32 sync word bits detected. 4 = No preamble/sync, carrier-sense above threshold. 5 = 15/16 + carrier-sense above threshold. 6 = 16/16 + carrier-sense above threshold. 7 = 30/32 + carrier-sense above threshold.\r\n"));
  // Add the rest of the help text here...
}

// Sets the modulation for the radio
void setModulation(int setting) {
  CC1.setModulation(setting);
  Serial.print(F("\r\nModulation: "));
  if (setting == 0) {
    Serial.print(F("2-FSK"));
  } else if (setting == 1) {
    Serial.print(F("GFSK"));
  } else if (setting == 2) {
    Serial.print(F("ASK/OOK"));
  } else if (setting == 3) {
    Serial.print(F("4-FSK"));
  } else if (setting == 4) {
    Serial.print(F("MSK"));
  }
  Serial.print(F(" \r\n"));
}

// Use to set specific frequency 
void setMhz(float settingf1) {
  CC1.setMHZ(settingf1);
  CC2.setMHZ(settingf1);
  Serial.print(F("\r\nFrequency: "));
  Serial.print(settingf1);
  Serial.print(F(" MHz\r\n"));
}

void setDeviation(float settingf1) {
  CC1.setDeviation(settingf1);
  Serial.print(F("\r\nDeviation: "));
  Serial.print(settingf1);
  Serial.print(F(" KHz\r\n"));
}

void setChannel(int setting) {
  CC1.setChannel(setting);
  Serial.print(F("\r\nChannel:"));
  Serial.print(setting);
  Serial.print(F("\r\n"));
}

void setChsp(float settingf1) {
  CC1.setChsp(settingf1);
  Serial.print(F("\r\nChann spacing: "));
  Serial.print(settingf1);
  Serial.print(F(" kHz\r\n"));
}

void setRxBw(float settingf1) {
  CC1.setRxBW(settingf1);
  Serial.print(F("\r\nRX bandwidth: "));
  Serial.print(settingf1);
  Serial.print(F(" kHz \r\n"));
}

void setDRate(float settingf1) {
  CC1.setDRate(settingf1);
  Serial.print(F("\r\nDatarate: "));
  Serial.print(settingf1);
  Serial.print(F(" kbaud\r\n"));
}

void setPa(int setting) {
  CC1.setPA(setting);
  Serial.print(F("\r\nTX PWR: "));
  Serial.print(setting);
  Serial.print(F(" dBm\r\n"));
}

void setSyncMode(int setting) {
  CC1.setSyncMode(setting);
  Serial.print(F("\r\nSynchronization: "));
  if (setting == 0) {
    Serial.print(F("No preamble"));
  } else if (setting == 1) {
    Serial.print(F("16 sync bits"));
  } else if (setting == 2) {
    Serial.print(F("16/16 sync bits"));
  } else if (setting == 3) {
    Serial.print(F("30/32 sync bits"));
  } else if (setting == 4) {
    Serial.print(F("No preamble/sync, carrier-sense"));
  } else if (setting == 5) {
    Serial.print(F("15/16 + carrier-sense"));
  } else if (setting == 6) {
    Serial.print(F("16/16 + carrier-sense"));
  } else if (setting == 7) {
    Serial.print(F("30/32 + carrier-sense"));
  }
  Serial.print(F("\r\n"));
}

void setSyncWord(int setting, int setting2) {
  CC1.setSyncWord(setting2, setting);
  Serial.print(F("\r\nSynchronization:\r\n"));
  Serial.print(F("high = "));
  Serial.print(setting);
  Serial.print(F("\r\nlow = "));
  Serial.print(setting2);
  Serial.print(F("\r\n"));
}

void setAdrChk(int setting) {
  CC1.setAdrChk(setting);
  Serial.print(F("\r\nAddress checking:"));
  if (setting == 0) {
    Serial.print(F("No adr chk"));
  } else if (setting == 1) {
    Serial.print(F("Adr chk, no bcast"));
  } else if (setting == 2) {
    Serial.print(F("Adr chk and 0 bcast"));
  } else if (setting == 3) {
    Serial.print(F("Adr chk and 0 and FF bcast"));
  }
  Serial.print(F("\r\n"));
}

void setAddr(int setting) {
  CC1.setAddr(setting);
  Serial.print(F("\r\nAddress: "));
  Serial.print(setting);
  Serial.print(F("\r\n"));
}

void setWhiteData(int setting) {
  CC1.setWhiteData(setting);
  Serial.print(F("\r\nWhitening "));
  if (setting == 0) {
    Serial.print(F("OFF"));
  } else if (setting == 1) {
    Serial.print(F("ON"));
  }
  Serial.print(F("\r\n"));
}

void setPktFormat(int setting) {
  CC1.setPktFormat(setting);
  Serial.print(F("\r\nPacket format: "));
  if (setting == 0) {
    Serial.print(F("Normal mode"));
  } else if (setting == 1) {
    Serial.print(F("Synchronous serial mode"));
  } else if (setting == 2) {
    Serial.print(F("Random TX mode"));
  } else if (setting == 3) {
    Serial.print(F("Asynchronous serial mode"));
  }
  Serial.print(F("\r\n"));
}

void setLengthConfig(int setting) {
  CC1.setLengthConfig(setting);
  Serial.print(F("\r\nPkt length mode: "));
  if (setting == 0) {
    Serial.print(F("Fixed"));
  } else if (setting == 1) {
    Serial.print(F("Variable"));
  } else if (setting == 2) {
    Serial.print(F("Infinite"));
  } else if (setting == 3) {
    Serial.print(F("Reserved"));
  }
  Serial.print(F("\r\n"));
}

void setPacketLength(int setting) {
  CC1.setPacketLength(setting);
  Serial.print(F("\r\nPkt length: "));
  Serial.print(setting);
  Serial.print(F(" bytes\r\n"));
}

void setCrc(int setting) {
  CC1.setCrc(setting);
  Serial.print(F("\r\nCRC checking: "));
  if (setting == 0) {
    Serial.print(F("Disabled"));
  } else if (setting == 1) {
    Serial.print(F("Enabled"));
  }
  Serial.print(F("\r\n"));
}

void setCrcAf(int setting) {
  CC1.setCRC_AF(setting);
  Serial.print(F("\r\nCRC Autoflush: "));
  if (setting == 0) {
    Serial.print(F("Disabled"));
  } else if (setting == 1) {
    Serial.print(F("Enabled"));
  }
  Serial.print(F("\r\n"));
}

void setDcFilterOff(int setting) {
  CC1.setDcFilterOff(setting);
  Serial.print(F("\r\nDC filter: "));
  if (setting == 0) {
    Serial.print(F("Enabled"));
  } else if (setting == 1) {
    Serial.print(F("Disabled"));
  }
  Serial.print(F("\r\n"));
}

void setManchester(int setting) {
  CC1.setManchester(setting);
  Serial.print(F("\r\nManchester coding: "));
  if (setting == 0) {
    Serial.print(F("Disabled"));
  } else if (setting == 1) {
    Serial.print(F("Enabled"));
  }
  Serial.print(F("\r\n"));
}

void setFec(int setting) {
  CC1.setFEC(setting);
  Serial.print(F("\r\nForward Error Correction: "));
  if (setting == 0) {
    Serial.print(F("Disabled"));
  } else if (setting == 1) {
    Serial.print(F("Enabled"));
  }
  Serial.print(F("\r\n"));
}

void setPre(int setting) {
  CC1.setPRE(setting);
  Serial.print(F("\r\nMinimum preamble bytes:"));
  Serial.print(setting);
  Serial.print(F(" means 0 = 2 bytes, 1 = 3b, 2 = 4b, 3 = 6b, 4 = 8b, 5 = 12b, 6 = 16b, 7 = 24 bytes\r\n"));
}

void setPqt(int setting) {
  CC1.setPQT(setting);
  Serial.print(F("\r\nPQT: "));
  Serial.print(setting);
  Serial.print(F("\r\n"));
}

void setAppendStatus(int setting) {
  CC1.setAppendStatus(setting);
  Serial.print(F("\r\nStatus bytes appending: "));
  if (setting == 0) {
    Serial.print(F("Enabled"));
  } else if (setting == 1) {
    Serial.print(F("Disabled"));
  }
  Serial.print(F("\r\n"));
}

void getRssi() {
  Serial.print(F("Rssi: "));
  Serial.println(CC1.getRssi());
  Serial.print(F(" LQI: "));
  Serial.println(CC1.getLqi());
  Serial.print(F("\r\n"));
  //displayInfo("Rssi & LQI: ", CC1.getRssi(), CC1.getLqi());
}



void scan(float settingf1, float settingf2) {
  SignalInfo foundSignals[MAX_SIGNALS];
  int signalCount = 0;

  Serial.print(F("\r\nScanning frequency range from : "));
  Serial.print(settingf1);
  Serial.print(F(" MHz to "));
  Serial.print(settingf2);
  Serial.print(F(" MHz, press any key to stop...\r\n"));

  // Initialize display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Scanning:");
  display.setCursor(0, 10);
  display.print(settingf1);
  display.print(" MHz to ");
  display.print(settingf2);
  display.print(" MHz");
  display.display();

  // Initialize CC1 for scanning
  CC1.Init();
  CC1.setRxBW(58);
  CC1.SetRx();

  float freq = settingf1;
  unsigned long displayUpdateTime = 0;
  const unsigned long DISPLAY_HOLD_TIME = 3000;  // 5 seconds to hold signal info

  while (!isButtonPressed(SELECT_BUTTON_PIN)) {
    CC1.setMHZ(freq);
    float rssi = CC1.getRssi();

    // Update display with current scanning frequency
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Scanning: ");
    display.print(freq, 2);  // Print with 2 decimal places
    display.print(" MHz");

    // Check for strong signals
    if (rssi > -75) {
      // Check if this signal is already in our list
      bool signalExists = false;
      for (int i = 0; i < signalCount; i++) {
        if (abs(foundSignals[i].frequency - freq) < 0.05) {
          signalExists = true;
          foundSignals[i].rssi = rssi;
          foundSignals[i].timestamp = millis();
          break;
        }
      }

      // If signal is new and we have space, add it
      if (!signalExists && signalCount < MAX_SIGNALS) {
        foundSignals[signalCount].frequency = freq;
        foundSignals[signalCount].rssi = rssi;
        foundSignals[signalCount].timestamp = millis();
        signalCount++;
      }

      // Print signal immediately
      Serial.print(F("\r\nSignal detected at "));
      Serial.print(F("Freq: "));
      Serial.print(freq, 2);
      Serial.print(F(" Rssi: "));
      Serial.println(rssi);
    }

    // Periodically update display with found signals
    if (millis() - displayUpdateTime > 500) {  // Update display every 500ms
      // Remove old signals
      unsigned long currentTime = millis();
      for (int i = 0; i < signalCount; i++) {
        if (currentTime - foundSignals[i].timestamp > DISPLAY_HOLD_TIME) {
          // Remove this signal by shifting the array
          for (int j = i; j < signalCount - 1; j++) {
            foundSignals[j] = foundSignals[j + 1];
          }
          signalCount--;
          i--;
        }
      }

      // Display found signals
      if (signalCount > 0) {
        display.setCursor(0, 10);
        display.print("Signals:");
        for (int i = 0; i < signalCount; i++) {
          display.setCursor(0, 20 + (i * 10));
          display.print(foundSignals[i].frequency, 2);
          display.print(" MHz ");
          display.print(foundSignals[i].rssi, 1);
          display.print(" dBm");
        }
      }

      display.display();
      displayUpdateTime = millis();
    }

    // Increment frequency
    freq += 0.05;  // Slightly larger increment for faster scanning

    // Reset scan if exceeded range
    if (freq > settingf2) {
      freq = settingf1;
    }

    // Small delay to prevent overwhelming the system
    delay(10);
  }
}

//add sd saving function
void save() {
  Serial.print(F("\r\nSaving recording buffer content into the non-volatile memory...\r\n"));
  for (int setting = 0; setting < EPROMSIZE; setting++) {
    EEPROM.write(setting, bigrecordingbuffer[setting]);
  }
  EEPROM.commit();
  Serial.print(F("\r\nSaving complete.\r\n\r\n"));
}

//add sd loading function
void load() {
  for (int setting = 0; setting < RECORDINGBUFFERSIZE; setting++) {
    bigrecordingbuffer[setting] = 0;
  }
  bigrecordingbufferpos = 0;
  framesinbigrecordingbuffer = 0;
  Serial.print(F("\r\nLoading content from the non-volatile memory into the recording buffer...\r\n"));
  for (int setting = 0; setting < EPROMSIZE; setting++) {
    bigrecordingbuffer[setting] = EEPROM.read(setting);
  }
  Serial.print(F("\r\nLoading complete. Enter 'show' or 'showraw' to see the buffer content.\r\n\r\n"));
}

void toggleRxMode() {
  Serial.print(F("\r\nReceiving and printing RF packet changed to "));
  if (receivingmode == 1) {
    receivingmode = 0;
    Serial.print(F("Disabled"));
  } else if (receivingmode == 0) {
    CC1.SetRx();
    Serial.print(F("Enabled"));
    receivingmode = 1;
    jammingmode = 0;
    recordingmode = 0;
  }
  Serial.print(F("\r\n"));
}

void toggleChatMode() {
  Serial.print(F("\r\nEntering chat mode:\r\n\r\n"));
  if (chatmode == 0) {
    chatmode = 1;
    jammingmode = 0;
    receivingmode = 0;
    recordingmode = 0;
  }
}

void toggleJammingMode() {
  int i = 0;

  Serial.print(F("\r\nJamming changed to "));
  if (jammingmode == 1) {
    Serial.print(F("Disabled"));
    jammingmode = 0;
  } else if (jammingmode == 0) {
    Serial.print(F("Enabled"));
    jammingmode = 1;
    receivingmode = 0;
    randomSeed(analogRead(0));
    for (i = 0; i < 60; i++) {
      ccsendingbuffer[i] = (byte)random(255);
    };
    // send these data to radio over CC1101
    CC1.SendData(ccsendingbuffer, 60);
    CC2.SendData(ccsendingbuffer, 60);
  }
  Serial.print(F("\r\n"));
}

void bruteForce(int setting, int setting2) {
  uint16_t brute, poweroftwo;
  if (setting > 0) {
    CC1.setCCMode(0);
    CC1.setPktFormat(3);
    CC1.SetTx();
    Serial.print(F("\r\nStarting Brute Forcing press any key to stop...\r\n"));
    pinMode(gdo0_1, OUTPUT);
    pinMode(gdo0_2, OUTPUT);
    int poweroftwo = 1 << setting2;
    for (brute = 0; brute < poweroftwo; brute++) {
      for (int k = 0; k < 5; k++)  // sending 5 times each code
      {
        for (int j = (setting2 - 1); j > -1; j--)  // j bits in a value brute
        {
          digitalWrite(gdo0_1, bitRead(brute, j));  // Set GDO0 according to actual brute force value
          digitalWrite(gdo0_2, bitRead(brute, j));  // Set GDO0 according to actual brute force value
          delayMicroseconds(setting);               // delay for selected sampling interval
        };                                          // end of J loop
      };                                            // end of K loop
      // checking if key pressed
      if (Serial.available())
        break;
    };
    Serial.print(F("\r\nBrute forcing complete.\r\n\r\n"));

    // setting normal pkt format again
    CC1.setCCMode(1);
    CC1.setPktFormat(0);
    CC1.SetTx();
    // pinMode(gdo0pin, INPUT);
  }  // end of IF
  else {
    Serial.print(F("Wrong parameters.\r\n"));
  };
}
/*
// Function to handle TX command
void transmitData(const char *hexData) {
  if ((strlen(hexData) <= 120) && (strlen(hexData) > 0)) {
    hextoascii(textbuffer, (byte *)hexData, strlen(hexData));
    memcpy(ccsendingbuffer, textbuffer, strlen(hexData) / 2);
    ccsendingbuffer[strlen(hexData) / 2] = 0x00;
    Serial.print("\r\nTransmitting RF packets.\r\n");

    // Send these data to radio over CC1101
        CC1.SendData(ccsendingbuffer, (byte)(strlen(hexData) / 2);

        // For DEBUG only
        asciitohex(ccsendingbuffer, textbuffer, strlen(hexData) / 2);
        Serial.print(F("Sent frame: "));
        Serial.print((char*)textbuffer);
        Serial.print(F("\r\n"));
  } else {
    Serial.print(F("Wrong parameters.\r\n"));
  }
}
*/
// Function to handle RECRAW command
void recordRawData(int interval) {
  if (interval > 0) {
    CC1.setCCMode(0);
    CC1.setPktFormat(3);
    CC1.SetRx();
    Serial.println(F("Waiting for radio signal to start RAW recording..."));
    updateDisplay("Waiting for signal...");

    pinMode(gdo0_1, INPUT);
    while (digitalRead(gdo0_1) == LOW)
      ;

    Serial.println(F("Starting RAW recording..."));
    updateDisplay("Recording RAW data...");

    for (int i = 0; i < RECORDINGBUFFERSIZE; i++) {
      byte receivedbyte = 0;
      for (int j = 7; j > -1; j--) {
        bitWrite(receivedbyte, j, digitalRead(gdo0_1));
        delayMicroseconds(interval);
      }
      bigrecordingbuffer[i] = receivedbyte;
    }

    Serial.println(F("Recording complete."));
    updateDisplay("Recording complete.");
  } else {
    Serial.println(F("Wrong parameters."));
    updateDisplay("Wrong parameters.");
  }
}

void sniffRawData(int interval) {
  if (interval > 0) {
    CC1.setCCMode(0);
    CC1.setPktFormat(3);
    CC1.SetRx();
    Serial.println(F("Sniffer enabled..."));
    updateDisplay("Sniffer enabled...");

    pinMode(gdo0_1, INPUT);
    while (!Serial.available()) {
      for (int i = 0; i < RECORDINGBUFFERSIZE; i++) {
        byte receivedbyte = 0;
        for (int j = 7; j > -1; j--) {
          bitWrite(receivedbyte, j, digitalRead(gdo0_1));
          delayMicroseconds(interval);
        }
        bigrecordingbuffer[i] = receivedbyte;
      }
    }
    Serial.println(F("Stopping the sniffer."));
    updateDisplay("Sniffer stopped.");
  } else {
    Serial.println(F("Wrong parameters."));
    updateDisplay("Wrong parameters.");
  }
}

void playRawData(int interval) {
  if (interval > 0) {
    CC1.setCCMode(0);
    CC1.setPktFormat(3);
    CC1.SetTx();
    Serial.println(F("Replaying RAW data..."));
    updateDisplay("Replaying RAW data...");

    pinMode(gdo0_1, OUTPUT);
    for (int i = 1; i < RECORDINGBUFFERSIZE; i++) {
      byte receivedbyte = bigrecordingbuffer[i];
      for (int j = 7; j > -1; j--) {
        digitalWrite(gdo0_1, bitRead(receivedbyte, j));
        delayMicroseconds(interval);
      }
    }

    Serial.println(F("Replaying complete."));
    updateDisplay("Replay complete.");
  } else {
    Serial.println(F("Wrong parameters."));
    updateDisplay("Wrong parameters.");
  }
}
// Function to handle SHOWRAW command
void showRawData() {
  Serial.print(F("\r\nRecorded RAW data:\r\n"));
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("RAW Data:"));
  int y = 10;
  for (int i = 0; i < RECORDINGBUFFERSIZE; i = i + 32) {
    asciitohex(&bigrecordingbuffer[i], textbuffer, 32);
    Serial.print((char *)textbuffer);
    display.setCursor(0, y);
    display.print((char *)textbuffer);
    y += 10;
    if (y > 50) break;  // Prevent overflow on screen
  }
  display.display();
  Serial.print(F("\r\n\r\n"));
}

// Shows recorded data in bits
void showBitData() {
  Serial.print(F("\r\nRecorded RAW data as bit stream:\r\n"));
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("Bit Stream:"));
  int y = 10;
  for (int i = 0; i < RECORDINGBUFFERSIZE; i = i + 32) {
    asciitohex((byte *)&bigrecordingbuffer[i], (byte *)textbuffer, 32);
    String bitStream = "";
    for (int setting = 0; setting < 64; setting++) {
      char setting2 = textbuffer[setting];
      switch (setting2) {
        case '0': bitStream += "____"; break;
        case '1': bitStream += "___-"; break;
        case '2': bitStream += "__-_"; break;
        case '3': bitStream += "__--"; break;
        case '4': bitStream += "_-__"; break;
        case '5': bitStream += "_-_-"; break;
        case '6': bitStream += "_--_"; break;
        case '7': bitStream += "_---"; break;
        case '8': bitStream += "-___"; break;
        case '9': bitStream += "-__-"; break;
        case 'A': bitStream += "-_-_"; break;
        case 'B': bitStream += "-_--"; break;
        case 'C': bitStream += "--__"; break;
        case 'D': bitStream += "--_-"; break;
        case 'E': bitStream += "---_"; break;
        case 'F': bitStream += "----"; break;
      }
    }
    Serial.print(bitStream);
    display.setCursor(0, y);
    display.print(bitStream.substring(0, 21));  // Limit to screen width
    y += 10;
    if (y > 50) break;
  }
  display.display();
  Serial.print(F("\r\n\r\n"));
}
// Function to handle ADDRAW command
void addRawData(const char *hexData) {
  int len = strlen(hexData);

  if ((len <= 120) && (len > 0)) {
    // Convert the hex content to array of bytes
    hextoascii(textbuffer, (byte *)hexData, len);
    len = len / 2;

    // Check if the frame fits into the buffer and store it
    if ((bigrecordingbufferpos + len) < RECORDINGBUFFERSIZE) {
      // Copy current frame and increase pointer for next frames
      memcpy(&bigrecordingbuffer[bigrecordingbufferpos], &textbuffer, len);
      // Increase position in big recording buffer for next frame
      bigrecordingbufferpos = bigrecordingbufferpos + len;
      Serial.print(F("\r\nChunk added to recording buffer\r\n\r\n"));
    } else {
      Serial.print(F("\r\nBuffer is full. The frame does not fit.\r\n "));
    }
  } else {
    Serial.print(F("Wrong parameters.\r\n"));
  }
}

// Function to handle REC command
void toggleRecordingMode() {
  Serial.print(F("\r\nRecording mode set to "));
  if (recordingmode == 1) {
    Serial.print(F("Disabled"));
    bigrecordingbufferpos = 0;
    recordingmode = 0;
  } else if (recordingmode == 0) {
    ELECHOUSE_cc1101.SetRx();
    Serial.print(F("Enabled"));
    bigrecordingbufferpos = 0;
    // Flush buffer for recording
    for (int i = 0; i < RECORDINGBUFFERSIZE; i++) {
      bigrecordingbuffer[i] = 0;
    }
    recordingmode = 1;
    jammingmode = 0;
    receivingmode = 0;
    // Start counting frames in the buffer
    framesinbigrecordingbuffer = 0;
  }
  Serial.print(F("\r\n"));
}

// Function to handle PLAY command
void playRecordedFrames(int frameNumber) {
  if (frameNumber <= framesinbigrecordingbuffer) {
    Serial.print(F("\r\nReplaying recorded frames.\r\n "));
    // Rewind recording buffer position to the beginning
    bigrecordingbufferpos = 0;

    if (framesinbigrecordingbuffer > 0) {
      // Start reading and sending frames from the buffer: FIFO
      for (int i = 1; i <= framesinbigrecordingbuffer; i++) {
        // Read length of the recorded frame first from the buffer
        int len = bigrecordingbuffer[bigrecordingbufferpos];
        if (((len <= 60) && (len > 0)) && ((i == frameNumber) || (frameNumber == 0))) {
          // Take next frame from the buffer for replay
          memcpy(ccsendingbuffer, &bigrecordingbuffer[bigrecordingbufferpos + 1], len);
          // Send these data to radio over CC1101
          ELECHOUSE_cc1101.SendData(ccsendingbuffer, (byte)len);
        }
        // Increase position to the buffer and check exception
        bigrecordingbufferpos = bigrecordingbufferpos + 1 + len;
        if (bigrecordingbufferpos > RECORDINGBUFFERSIZE)
          break;
      }
    }
    // Rewind buffer position
    bigrecordingbufferpos = 0;
    Serial.print(F("Done.\r\n"));
  } else {
    Serial.print(F("Wrong parameters.\r\n"));
  }
}

// Function to handle ADD command
void addFrame(const char *hexData) {
  int len = strlen(hexData);

  if ((len <= 120) && (len > 0)) {
    // Convert the hex content to array of bytes
    hextoascii(textbuffer, (byte *)hexData, len);
    len = len / 2;

    // Check if the frame fits into the buffer and store it
    if ((bigrecordingbufferpos + len + 1) < RECORDINGBUFFERSIZE) {
      // Put info about number of bytes
      bigrecordingbuffer[bigrecordingbufferpos] = len;
      bigrecordingbufferpos++;
      // Next - copy current frame and increase
      memcpy(&bigrecordingbuffer[bigrecordingbufferpos], &textbuffer, len);
      // Increase position in big recording buffer for next frame
      bigrecordingbufferpos = bigrecordingbufferpos + len;
      // Increase counter of frames stored
      framesinbigrecordingbuffer++;
      Serial.print(F("\r\nAdded frame number "));
      Serial.print(framesinbigrecordingbuffer);
      Serial.print(F("\r\n"));
    } else {
      Serial.print(F("\r\nBuffer is full. The frame does not fit.\r\n "));
    }
  } else {
    Serial.print(F("Wrong parameters.\r\n"));
  }
}

// Function to handle SHOW command
void showRecordedFrames() {
  if (framesinbigrecordingbuffer > 0) {
    Serial.print(F("\r\nFrames stored in the recording buffer:\r\n "));
    // Rewind recording buffer position to the beginning
    bigrecordingbufferpos = 0;

    // Start reading and sending frames from the buffer: FIFO
    for (int setting = 1; setting <= framesinbigrecordingbuffer; setting++) {
      // Read length of the recorded frame first from the buffer
      int len = bigrecordingbuffer[bigrecordingbufferpos];
      if ((len <= 60) && (len > 0)) {
        // Take next frame from the buffer for replay
        // Flush textbuffer
        for (int setting2 = 0; setting2 < BUF_LENGTH; setting2++) {
          textbuffer[setting2] = 0;
        }
        asciitohex(&bigrecordingbuffer[bigrecordingbufferpos + 1], textbuffer, len);
        Serial.print(F("\r\nFrame "));
        Serial.print(setting);
        Serial.print(F(" : "));
        Serial.print((char *)textbuffer);
        Serial.print(F("\r\n"));
      }
      // Increase position to the buffer and check exception
      bigrecordingbufferpos = bigrecordingbufferpos + 1 + len;
      if (bigrecordingbufferpos > RECORDINGBUFFERSIZE)
        break;
    }
    Serial.print(F("\r\n"));
  } else {
    Serial.print(F("Wrong parameters.\r\n"));
  }
}

// Function to handle FLUSH command and clear the buffer 
void flushRecordingBuffer() {
  // Flushing bigrecordingbuffer with zeros and rewinding all the pointers
  for (int setting = 0; setting < RECORDINGBUFFERSIZE; setting++) {
    bigrecordingbuffer[setting] = 0;
  }
  // Rewinding all the pointers to the recording buffer
  bigrecordingbufferpos = 0;
  framesinbigrecordingbuffer = 0;
  Serial.print(F("\r\nRecording buffer cleared.\r\n"));
}

// Function to handle ECHO command
void setEchoMode(int mode) {
  do_echo = mode;
}

// Function to handle X command
void stopAllModes() {
  receivingmode = 0;
  jammingmode = 0;
  recordingmode = 0;
  Serial.print(F("\r\n"));
}

// Function to handle INIT command of both CC1101 radios
void initializeCC1101() {
  // Initialize CC1101
  cc1101initialize();
  cc1101initialize_2();
  display.clearDisplay();
  u8g2_for_adafruit_gfx.setFont(u8g2_font_baby_tf);

  if (CC1.getCC1101()) {  // Check the CC1101 Spi connection.
    Serial.println(F("cc1101 #1 initialized. Connection OK\n\r"));
    u8g2_for_adafruit_gfx.setCursor(0, 30);
    u8g2_for_adafruit_gfx.print("cc1101 initialized. Connection OK");
    display.display();
    nonBlockingDelay(1000);
  } else {
    Serial.println(F("cc1101 #1 connection error! check the wiring.\n\r"));
    u8g2_for_adafruit_gfx.setCursor(0, 30);
    u8g2_for_adafruit_gfx.print("cc1101 connection ERROR!. Connection OK");
    u8g2_for_adafruit_gfx.setCursor(0, 40);
    u8g2_for_adafruit_gfx.print("CHECK WIRING!");

    display.display();
    nonBlockingDelay(1000);
  };
  if (CC2.getCC1101()) {  // Check the CC1101 Spi connection.
    Serial.println(F("cc1101 #2 initialized. Connection OK\n\r"));
    u8g2_for_adafruit_gfx.setCursor(0, 40);
    u8g2_for_adafruit_gfx.print("cc1101 initialized. Connection OK");
    display.display();
    nonBlockingDelay(1000);
  } else {
    Serial.println(F("cc1101 #1 connection error! check the wiring.\n\r"));
    u8g2_for_adafruit_gfx.setCursor(0, 40);
    u8g2_for_adafruit_gfx.print("cc1101 connection ERROR!. Connection OK");
    u8g2_for_adafruit_gfx.setCursor(0, 50);
    u8g2_for_adafruit_gfx.print("CHECK WIRING!");
    display.display();
    nonBlockingDelay(1000);
  };

  // Give feedback
  Serial.print(F("CC1101 initialized\r\n"));
}

// ------- CC1101 SETUP END ------------

// **** Menu Functions ****

// Handles executing functions and calling the related functions
void executeSelectedMenuItem() {
  switch (selectedMenuItem) {

    case BT_JAM:
      currentState = STATE_BT_JAM;
      Serial.println("BT JAM button pressed");
      displayInfo("BT JAMMER", "TURN ON LEFT SWITCH", "Starting....");
      initRadios();
      initRadiosHspi();
      nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      displayInfo("BT JAMMER", "RADIOS ACTIVE", "Running....");
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        btJam();
      }
      break;

    case WIFI_JAM:
      currentState = STATE_WIFI_JAM;
      Serial.println("WIFI JAM button pressed");
      displayInfo("WIFI JAMMER", "TURN ON LEFT SWITCH", "Starting....");
      initRadios();
      initRadiosHspi();
      nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      displayInfo("WIFI JAMMER", "RADIOS ACTIVE", "Running....");
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        wifiJam();
      }
      break;
    case DRONE_JAM:
      currentState = STATE_DRONE_JAM;
      Serial.println("DRONE JAM button pressed");
      displayInfo("DRONE JAMMER", "TURN ON LEFT SWITCH", "Starting....");
      initRadios();
      initRadiosHspi();
      nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      displayInfo("DRONE JAMMER", "RADIOS ACTIVE", "Running....");
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        droneJam();
      }
      break;
    case NRF_SCAN:
      currentState = STATE_NRF_SCAN;
      Serial.println("NRF_SCAN button pressed");
      displayInfo("NRF24 SCAN", "TURN ON LEFT SWITCH", "Scanning....");
      analyzerSetup();
      nonBlockingDelay(4000);  // Debounce nonBlockingDelay
      break;
    case TEST_NRF:
      currentState = STATE_TEST_NRF;
      Serial.println("TEST_NRF button pressed");
      displayInfo("NRF24 TEST", "TURN ON LEFT SWITCH", "Starting....");
      initRadios();
      nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      break;
    case TEST_CC1101:
      currentState = STATE_TEST_NRF;
      Serial.println("TEST_CC1101 button pressed");
      displayInfo("CC1101 TEST", "TURN ON LEFT SWITCH", "Starting....");
      initializeCC1101();
      nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      // displayInfo("TESTING CC1101s", "RADIOS ACTIVE", "Running....");
      break;
      // testing CC1 & CC2(on schematic) slots
    case TEST_HSPI:
      currentState = STATE_TEST_NRF;
      Serial.println("TEST_CC1101 button pressed");
      displayInfo("CC1101 TEST", "TURN ON LEFT SWITCH", "Starting....");
      initRadiosHspi();
      nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      break;
    case CC1_JAM:
      currentState = STATE_CC1_JAM;
      Serial.println("CC1 JAM button pressed");
      displayInfo("433hz JAMMER", "Activating Radio", "Starting....");
      //toggleJammingMode();
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("CC1101 JAMMER", "RADIOS ACTIVE", "Running....");
      }
      break;
    case CC1_SINGLE:
      currentState = STATE_CC1_SINGLE;
      Serial.println("CC1 SINGLE button pressed");
      displayInfo("433hz CC#1 JAMMER", "Activating Radio", "Starting....");
      //toggleJammingMode();
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("CC1101 JAMMER", "CC#1 RADIO ACTIVE", "Running....");
      }
      break;
    case CC2_SINGLE:
      currentState = STATE_CC2_SINGLE;
      Serial.println("CC2 SINGLE button pressed");
      displayInfo("433hz CC#2 JAMMER", "Activating Radio", "Starting....");
      //toggleJammingMode();
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("CC1101 JAMMER", "CC#2 RADIO ACTIVE", "Running....");
      }
      break;
    case REC_RAW:
      currentState = STATE_REC_RAW;
      Serial.println("REC_RAW button pressed");
      displayInfo("REC_RAW", "recording raw data", "Recording....");
      recordRawData(100);
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      break;
    case CC_SCAN:
      currentState = STATE_CC_SCAN;
      Serial.println("CC_SCAN button pressed");
      displayInfo("CC_SCAN", "Scanning raw data", "Scanning....");
      scan(433.60, 434.20);
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      //while (!isButtonPressed(SELECT_BUTTON_PIN)) {
      //displayInfo("CC_SCAN", "Scanning raw data", "Scanning....");
      //}
      break;
    case PLAY_RAW:
      currentState = STATE_PLAY_RAW;
      Serial.println("PLAY_RAW button pressed");
      displayInfo("PLAY_RAW", "Playing raw data", "Playing....");
      playRawData(100);
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      break;
    case SHOW_RAW:
      currentState = STATE_SHOW_RAW;
      Serial.println("SHOW_RAW button pressed");
      displayInfo("SHOW_RAW", "Showing raw data", "Raw data....");
      showRawData();
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      break;
    case SHOW_BUFF:
      currentState = STATE_SHOW_BUFF;
      Serial.println("SHOW_BUFF button pressed");
      displayInfo("SHOW_BUFF", "Showing buffer data", "Raw buffer....");
      showBitData();
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      break;
    case FLUSH_BUFF:
      currentState = STATE_FLUSH_BUFF;
      Serial.println("FLUSH_BUFF button pressed");
      displayInfo("FLUSH_BUFF", "Clearing buffer data", "Clearing buffer....");
      flushRecordingBuffer();
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("FLUSH_BUFF", "Clearing buffer data", "Clearing buffer....");
      }
      break;
    case GET_RSSI:
      currentState = STATE_GET_RSSI;
      Serial.println("GET_RSSI button pressed");
      displayInfo("GET_RSSI", "Showing buffer data", "GETTING RSSI....");
      getRssi();

      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
    case STOP_ALL:
      currentState = STATE_STOP_ALL;
      Serial.println("STOP_ALL button pressed");
      displayInfo("STOP_ALL", "Stopping all actions", "Stopping....");
      stopAllModes();
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("STOP_ALL", "Stopping all actions", "Stopping....");
      }
      break;
    case RESET_CC:
      currentState = STATE_SHOW_BUFF;
      Serial.println("SHOW_BUFF button pressed");
      displayInfo("SHOW_BUFF", "Showing buffer data", "Raw buffer....");
      cc1101initialize();
      cc1101initialize_2();
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("SHOW_BUFF", "Showing buffer data", "Raw buffer....");
      }
      break;
    case SET_43400:
      currentState = STATE_SET_43400;
      Serial.println("SET_43400 button pressed");
      displayInfo("SET_43400", "FREQ SET", "434.00MHz....");
      setMhz(434.00);
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("SET_43400", "FREQ SET", "434.00MHz....");
      }
      break;
    case SET_43430:
      currentState = STATE_SET_43430;
      Serial.println("SET_43430 button pressed");
      displayInfo("SET_43430", "FREQ SET", "434.30MHz....");
      setMhz(434.30);
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("SET_43430", "FREQ SET", "434.30MHz....");
      }
      break;
    case SET_43440:
      currentState = STATE_SET_43440;
      Serial.println("SET_43440 button pressed");
      displayInfo("SET_43440", "FREQ SET", "434.40MHz....");
      setMhz(434.40);
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("SET_43440", "FREQ SET", "434.40MHz....");
      }
      break;
    case SET_43390:
      currentState = STATE_SET_43390;
      Serial.println("SET_43390 button pressed");
      displayInfo("SET_43390", "FREQ SET", "433.90MHz....");
      setMhz(433.90);
      // nonBlockingDelay(2000);  // Debounce nonBlockingDelay
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        displayInfo("SET_43390", "FREQ SET", "433.90MHz....");
      }
      break;
  }
}

// SETUP + LOOP
void setup() {
  Serial.begin(115200);
  delay(3000);
  initDisplay();
  // Disable unnecessary wireless interfaces
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();

  pinMode(UP_BUTTON_PIN, INPUT);
  pinMode(DOWN_BUTTON_PIN, INPUT);
  pinMode(SELECT_BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  // Initialize U8g2_for_Adafruit_GFX
  u8g2_for_adafruit_gfx.begin(display);

  // Display splash screens
  demonSHIT();
  delay(2000);  // Show title screen for 3 seconds
                // displayInfoScreen();
                // delay(5000);  // Show info screen for 5 seconds
  // Initial display

  display.clearDisplay();
  u8g2_for_adafruit_gfx.setFont(u8g2_font_baby_tf);
  cc1101initialize();
  cc1101initialize_2();

  if (CC1.getCC1101()) {  // Check the CC1101 Spi connection.
    Serial.println(F("cc1101 #1 initialized. Connection OK\n\r"));
    u8g2_for_adafruit_gfx.setCursor(0, 30);
    u8g2_for_adafruit_gfx.print("cc1101 initialized. Connection OK");
    display.display();
    delay(3000);
  } else {
    Serial.println(F("cc1101 #1 connection error! check the wiring.\n\r"));
    u8g2_for_adafruit_gfx.setCursor(0, 30);
    u8g2_for_adafruit_gfx.print("cc1101 connection ERROR!. Connection OK");
    u8g2_for_adafruit_gfx.setCursor(0, 40);
    u8g2_for_adafruit_gfx.print("CHECK WIRING!");

    display.display();
    delay(3000);
  };
  if (CC2.getCC1101()) {  // Check the CC1101 Spi connection.
    Serial.println(F("cc1101 #2 initialized. Connection OK\n\r"));
    u8g2_for_adafruit_gfx.setCursor(0, 40);
    u8g2_for_adafruit_gfx.print("cc1101 initialized. Connection OK");
    display.display();
    delay(3000);
  } else {
    Serial.println(F("cc1101 #1 connection error! check the wiring.\n\r"));
    u8g2_for_adafruit_gfx.setCursor(0, 40);
    u8g2_for_adafruit_gfx.print("cc1101 connection ERROR!. Connection OK");
    u8g2_for_adafruit_gfx.setCursor(0, 50);
    u8g2_for_adafruit_gfx.print("CHECK WIRING!");
    display.display();
    delay(3000);
  };
  drawMenu();
}

void loop() {
  switch (currentState) {
    case STATE_MENU:
      handleMenuSelection();
      break;
    case STATE_BT_JAM:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_WIFI_JAM:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_DRONE_JAM:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
      /*
  case STATE_CC1_JAM:
    if (isButtonPressed(SELECT_BUTTON_PIN)) {
      currentState = STATE_MENU;
      drawMenu();
      nonBlockingDelay(500);  // Debounce nonBlockingDelay
      return;
    }
    break;
    */
    case STATE_TEST_NRF:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_TEST_CC1101:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_REC_RAW:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_PLAY_RAW:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_SHOW_RAW:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_SHOW_BUFF:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_FLUSH_BUFF:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_GET_RSSI:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_CC_SCAN:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_NRF_SCAN:
      analyzerLoop();
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_CC1_JAM:
      Serial.println(F("Jamming Mode Activated"));

      jammingmode = 1;    // Ensure jamming mode is set
      receivingmode = 0;  // Disable receiving mode
      delay(1000);

      while (jammingmode) {  // Stay in this loop until the button is pressed again
        if (isButtonPressed(SELECT_BUTTON_PIN)) {
          Serial.println(F("Exiting Jamming Mode"));
          jammingmode = 0;
          currentState = STATE_MENU;
          drawMenu();
          nonBlockingDelay(500);  // Debounce
          return;
        }

        // Send random RF data continuously
        randomSeed(analogRead(0));
        for (int i = 0; i < 60; i++) {
          ccsendingbuffer[i] = (byte)random(255);
        }
        CC1.SendData(ccsendingbuffer, 60);
        CC2.SendData(ccsendingbuffer, 60);

        nonBlockingDelay(10);  // Adjust transmission speed
      }
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
      break;
    case STATE_CC1_SINGLE:
      Serial.println(F("Jamming Mode Activated"));

      jammingmode = 1;    // Ensure jamming mode is set
      receivingmode = 0;  // Disable receiving mode
      delay(1000);

      while (jammingmode) {  // Stay in this loop until the button is pressed again
        if (isButtonPressed(SELECT_BUTTON_PIN)) {
          Serial.println(F("Exiting Jamming Mode"));
          jammingmode = 0;
          currentState = STATE_MENU;
          drawMenu();
          nonBlockingDelay(500);  // Debounce
          return;
        }

        // Send random RF data continuously
        randomSeed(analogRead(0));
        for (int i = 0; i < 60; i++) {
          ccsendingbuffer[i] = (byte)random(255);
        }
        CC1.SendData(ccsendingbuffer, 60);

        nonBlockingDelay(10);  // Adjust transmission speed
      }

      break;
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
    case STATE_CC2_SINGLE:
      Serial.println(F("Jamming Mode Activated"));

      jammingmode = 1;    // Ensure jamming mode is set
      receivingmode = 0;  // Disable receiving mode
      delay(1000);
      while (jammingmode) {  // Stay in this loop until the button is pressed again
        if (isButtonPressed(SELECT_BUTTON_PIN)) {
          Serial.println(F("Exiting Jamming Mode"));
          jammingmode = 0;
          currentState = STATE_MENU;
          drawMenu();
          nonBlockingDelay(500);  // Debounce
          return;
        }

        // Send random RF data continuously
        randomSeed(analogRead(0));
        for (int i = 0; i < 60; i++) {
          ccsendingbuffer[i] = (byte)random(255);
        }
        CC2.SendData(ccsendingbuffer, 60);

        nonBlockingDelay(10);  // Adjust transmission speed
      }

      break;
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(500);  // Debounce nonBlockingDelay
        return;
      }
  }
  int i = 0;

  /* Process incoming commands. */
  while (Serial.available()) {
    static char buffer[BUF_LENGTH];
    static int length = 0;

    // handling CHAT MODE
    if (chatmode == 1) {

      // clear serial port buffer index
      i = 0;

      // something was received over serial port put it into radio sending buffer
      while (Serial.available() and (i < (CCBUFFERSIZE - 1))) {
        // read single character from Serial port
        ccsendingbuffer[i] = Serial.read();

        // also put it as ECHO back to serial port
        Serial.write(ccsendingbuffer[i]);

        // if CR was received add also LF character and display it on Serial port
        if (ccsendingbuffer[i] == 0x0d) {
          Serial.write(0x0a);
          i++;
          ccsendingbuffer[i] = 0x0a;
        }
        //

        // increase CC1101 TX buffer position
        i++;
      };

      // put NULL at the end of CC transmission buffer
      ccsendingbuffer[i] = '\0';

      // send these data to radio over CC1101
      CC1.SendData((char *)ccsendingbuffer);
    }
    // handling CLI commands processing
    else {
      int data = Serial.read();
      if (data == '\b' || data == '\177') {  // BS and DEL
        if (length) {
          length--;
          if (do_echo)
            Serial.write("\b \b");
        }
      } else if (data == '\r' || data == '\n') {
        if (do_echo)
          Serial.write("\r\n");  // output CRLF
        buffer[length] = '\0';
      } else if (length < BUF_LENGTH - 1) {
        buffer[length++] = data;
        if (do_echo)
          Serial.write(data);
      }
    };
    // end of handling CLI processing
  };

  /* Process RF received packets */

  // Checks whether something has been received.
  if (CC1.CheckReceiveFlag() && (receivingmode == 1 || recordingmode == 1 || chatmode == 1)) {

    // CRC Check. If "setCrc(false)" crc returns always OK!
    if (CC1.CheckCRC()) {
      // Get received Data and calculate length
      int len = CC1.ReceiveData(ccreceivingbuffer);

      // Actions for CHAT MODE
      if ((chatmode == 1) && (len < CCBUFFERSIZE)) {
        // put NULL at the end of char buffer
        ccreceivingbuffer[len] = '\0';
        // Print received in char format.
        Serial.print((char *)ccreceivingbuffer);
      };  // end of handling Chat mode

      // Actions for RECEIVNG MODE
      if (((receivingmode == 1) && (recordingmode == 0)) && (len < CCBUFFERSIZE)) {
        // put NULL at the end of char buffer
        ccreceivingbuffer[len] = '\0';
        // flush textbuffer
        for (int i = 0; i < BUF_LENGTH; i++) {
          textbuffer[i] = 0;
        };

        // Print received packet as set of hex values directly
        //  not to loose any data in buffer
        //  asciitohex((byte *)ccreceivingbuffer, (byte *)textbuffer,  len);
        asciitohex(ccreceivingbuffer, textbuffer, len);
        Serial.print((char *)textbuffer);
        // set RX  mode again
        CC1.SetRx();
      };  // end of handling receiving mode

      // Actions for RECORDING MODE
      if (((recordingmode == 1) && (receivingmode == 0)) && (len < CCBUFFERSIZE)) {
        // copy the frame from receiving buffer for replay - only if it fits
        if ((bigrecordingbufferpos + len + 1) < RECORDINGBUFFERSIZE) {  // put info about number of bytes
          bigrecordingbuffer[bigrecordingbufferpos] = len;
          bigrecordingbufferpos++;
          // next - copy current frame and increase
          memcpy(&bigrecordingbuffer[bigrecordingbufferpos], ccreceivingbuffer, len);
          // increase position in big recording buffer for next frame
          bigrecordingbufferpos = bigrecordingbufferpos + len;
          // increase counter of frames stored
          framesinbigrecordingbuffer++;
          // set RX  mode again
          CC1.SetRx();
          Serial.print("\r\nAdded frame number ");
          Serial.print(framesinbigrecordingbuffer);
          Serial.print("\r\n");
        }

        else {
          Serial.print(F("Recording buffer full! Stopping..\r\nFrames stored: "));
          Serial.print(framesinbigrecordingbuffer);
          Serial.print(F("\r\n"));
          bigrecordingbufferpos = 0;
          recordingmode = 0;
        };

      };  // end of handling frame recording mode

    };  // end of CRC check IF

  };  // end of Check receive flag if
}