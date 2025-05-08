/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include <Arduino.h>
#include "analyzer.h"

// Replace the u8g2 declaration with Adafruit GFX and U8G2 adapter

extern Adafruit_SSD1306 display;
extern U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;
// NRF24L01 Register definitions
#define NRF24_CONFIG      0x00
#define NRF24_EN_AA       0x01
#define NRF24_EN_RXADDR   0x02
#define NRF24_SETUP_AW    0x03
#define NRF24_RF_CH       0x05
#define NRF24_RF_SETUP    0x06
#define NRF24_STATUS      0x07
#define NRF24_RPD         0x09

// Display dimensions
#define SCREEN_WIDTH      128
#define SCREEN_HEIGHT     64

// GPIO pins
#define CE_PIN            27
#define CSN_PIN           15
#define UP_BUTTON         39
#define DOWN_BUTTON       34

// SPI pins
#define SPI_SCK           18
#define SPI_MISO          19
#define SPI_MOSI          23
#define SPI_SS            17

// Analyzer settings
#define MAX_CHANNELS      128       // Total available channels
#define HISTORY_SIZE      120       // Number of historical values to keep (for graph)
#define SIGNAL_MAX        10        // Maximum signal strength value
#define SCAN_INTERVAL     10        // Time between scans in milliseconds

// Global variables
uint8_t currentChannel = 0;         // Currently selected channel
uint8_t signalHistory[HISTORY_SIZE]; // Signal history for graphing
uint8_t historyIndex = 0;           // Current position in history buffer
uint8_t maxSignal = 0;              // Maximum signal level detected

unsigned long lastButtonCheck = 0;
unsigned long lastChannelScan = 0;
unsigned long showStartupUntil = 0;

// SPI communication functions
uint8_t readRegister(uint8_t reg) {
  digitalWrite(CSN_PIN, LOW);
  SPI.transfer(reg & 0x1F);
  uint8_t result = SPI.transfer(0x00);
  digitalWrite(CSN_PIN, HIGH);
  return result;
}

void writeRegister(uint8_t reg, uint8_t value) {
  digitalWrite(CSN_PIN, LOW);
  SPI.transfer((reg & 0x1F) | 0x20);
  SPI.transfer(value);
  digitalWrite(CSN_PIN, HIGH);
}

// NRF24 control functions
void setChannel(uint8_t channel) {
  writeRegister(NRF24_RF_CH, channel);
  currentChannel = channel;
}

void powerUp() {
  uint8_t config = readRegister(NRF24_CONFIG);
  // Set PWR_UP bit
  writeRegister(NRF24_CONFIG, config | 0x02);
  delay(2);
}

void configureReceiver() {
  // Set to RX mode, disable CRC (we're just listening passively)
  writeRegister(NRF24_CONFIG, 0x03);  // PWR_UP=1, PRIM_RX=1, CRC=0
  
  // Disable auto-acknowledgment
  writeRegister(NRF24_EN_AA, 0x00);
  
  // Configure address width to minimum
  writeRegister(NRF24_SETUP_AW, 0x01);
  
  // Set max power, 2Mbps rate (more sensitivity)
  writeRegister(NRF24_RF_SETUP, 0x0F);
  
  // Enable RX addresses
  writeRegister(NRF24_EN_RXADDR, 0x01);
}

void startReceiving() {
  digitalWrite(CE_PIN, HIGH);
}

void stopReceiving() {
  digitalWrite(CE_PIN, LOW);
}

// Check for RF energy on current channel
uint8_t detectSignalStrength() {
  uint8_t strength = 0;
  
  // Do multiple samples to get more accurate reading
  for (int i = 0; i < 5; i++) {
    startReceiving();
    delayMicroseconds(200);
    
    // Check RPD (Received Power Detector)
    if (readRegister(NRF24_RPD) & 0x01) {
      strength += 2; // Increment by 2 for more visible changes
    }
    
    stopReceiving();
    delayMicroseconds(50);
  }
  
  // Cap the maximum value
  if (strength > SIGNAL_MAX) {
    strength = SIGNAL_MAX;
  }
  
  return strength;
}

// Add a new signal measurement to history
void addSignalToHistory(uint8_t signal) {
  signalHistory[historyIndex] = signal;
  
  // Update maximum value if needed
  if (signal > maxSignal) {
    maxSignal = signal;
  } else {
    // Recalculate max if we're at a full cycle
    if (historyIndex == 0) {
      maxSignal = 0;
      for (int i = 0; i < HISTORY_SIZE; i++) {
        if (signalHistory[i] > maxSignal) {
          maxSignal = signalHistory[i];
        }
      }
    }
  }
  
  // Move to next position
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
}

// Scan the current channel
void performChannelScan() {
  if (millis() - lastChannelScan < SCAN_INTERVAL) {
    return;
  }
  
  lastChannelScan = millis();
  
  // Ensure we're on the right channel
  setChannel(currentChannel);
  
  // Measure signal strength
  uint8_t signal = detectSignalStrength();
  
  // Add to history
  addSignalToHistory(signal);
}

// Handle button presses
void checkButtons() {
  // Only check periodically to prevent excessive checking
  if (millis() - lastButtonCheck < 100) {
    return;
  }
  lastButtonCheck = millis();

  // Read button states (inverted since they're INPUT_PULLUP)
  bool upPressed = (digitalRead(UP_BUTTON) == LOW);
  bool downPressed = (digitalRead(DOWN_BUTTON) == LOW);

  // Handle up button - increment channel
  if (upPressed) {
    if (currentChannel < MAX_CHANNELS - 1) {
      currentChannel++;
    } else {
      currentChannel = 0; // Wrap around
    }
    
    // Clear history when changing channels
    memset(signalHistory, 0, sizeof(signalHistory));
    maxSignal = 0;
    historyIndex = 0;
    
    // Set the new channel
    setChannel(currentChannel);
    delay(100); // Simple debounce
  }

  // Handle down button - decrement channel
  if (downPressed) {
    if (currentChannel > 0) {
      currentChannel--;
    } else {
      currentChannel = MAX_CHANNELS - 1; // Wrap around
    }
    
    // Clear history when changing channels
    memset(signalHistory, 0, sizeof(signalHistory));
    maxSignal = 0;
    historyIndex = 0;
    
    // Set the new channel
    setChannel(currentChannel);
    delay(100); // Simple debounce
  }
}

// Draw the signal history graph for a single channel
void drawChannelGraph() {
  display.clearDisplay();
  
  // Draw title and channel info
  u8g2_for_adafruit_gfx.setFont(u8g2_font_ncenB10_tr);
  u8g2_for_adafruit_gfx.setCursor(0, 10);
  u8g2_for_adafruit_gfx.print("Channel: ");
  u8g2_for_adafruit_gfx.print(currentChannel);
  
  // Draw WiFi channel indicator if this is a standard WiFi channel
  if (currentChannel == 1 || currentChannel == 6 || currentChannel == 11) {
    u8g2_for_adafruit_gfx.setCursor(90, 10);
    u8g2_for_adafruit_gfx.print("WiFi");
  }
  
  // Draw horizontal line separating header from graph
  display.drawFastHLine(0, 12, SCREEN_WIDTH, SSD1306_WHITE);
  
  // Draw y-axis scale
  u8g2_for_adafruit_gfx.setFont(u8g2_font_profont10_mr);
  u8g2_for_adafruit_gfx.setCursor(0, 22);
  u8g2_for_adafruit_gfx.print("Max");
  u8g2_for_adafruit_gfx.setCursor(0, SCREEN_HEIGHT - 2);
  u8g2_for_adafruit_gfx.print("Min");
  
  // Draw vertical line for y-axis
  display.drawFastVLine(20, 13, SCREEN_HEIGHT - 13, SSD1306_WHITE);
  
  // Calculate scaling factors
  float xScale = (float)(SCREEN_WIDTH - 25) / HISTORY_SIZE;
  float yScale = (float)(SCREEN_HEIGHT - 15) / SIGNAL_MAX;
  
  // Draw signal history graph
  for (int i = 0; i < HISTORY_SIZE - 1; i++) {
    int idx1 = (historyIndex + i) % HISTORY_SIZE;
    int idx2 = (historyIndex + i + 1) % HISTORY_SIZE;
    
    int x1 = 22 + i * xScale;
    int x2 = 22 + (i + 1) * xScale;
    
    int y1 = SCREEN_HEIGHT - 2 - (signalHistory[idx1] * yScale);
    int y2 = SCREEN_HEIGHT - 2 - (signalHistory[idx2] * yScale);
    
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
  
  // Draw current signal strength as a bar on the right
  uint8_t currentSignal = signalHistory[(historyIndex == 0) ? HISTORY_SIZE - 1 : historyIndex - 1];
  int barHeight = currentSignal * yScale;
  display.fillRect(SCREEN_WIDTH - 10, SCREEN_HEIGHT - 2 - barHeight, 8, barHeight, SSD1306_WHITE);
  
  // Show numeric signal strength
  u8g2_for_adafruit_gfx.setCursor(SCREEN_WIDTH - 40, 22);
  u8g2_for_adafruit_gfx.print("Sig:");
  u8g2_for_adafruit_gfx.print(currentSignal);
  
  // Draw channel up/down indicators
  u8g2_for_adafruit_gfx.setCursor(0, SCREEN_HEIGHT);
  u8g2_for_adafruit_gfx.print("CH- [DOWN]    [UP] CH+");
  
  display.display();
}

// Show startup screen
void showStartupScreen() {
  display.clearDisplay();
  
  // Display title
  u8g2_for_adafruit_gfx.setFont(u8g2_font_ncenB14_tr);
  u8g2_for_adafruit_gfx.setCursor(5, 20);
  u8g2_for_adafruit_gfx.print("NRF24 Scanner");
  
  // Display mode info
  u8g2_for_adafruit_gfx.setFont(u8g2_font_profont10_mr);
  u8g2_for_adafruit_gfx.setCursor(20, 35);
  u8g2_for_adafruit_gfx.print("Single Channel Mode");
  
  // Display instructions
  u8g2_for_adafruit_gfx.setCursor(10, 50);
  u8g2_for_adafruit_gfx.print("UP/DOWN: Change channel");
  
  display.display();
}

// Setup function
void analyzerSetup() {
  Serial.begin(115200);
  Serial.println("NRF24 WiFi Single Channel Analyzer");
  
  // Disable WiFi and Bluetooth to avoid interference
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  
  // Initialize pins
  pinMode(CE_PIN, OUTPUT);
  pinMode(CSN_PIN, OUTPUT);
  pinMode(UP_BUTTON, INPUT_PULLUP);
  pinMode(DOWN_BUTTON, INPUT_PULLUP);
  
  // Initial pin states
  digitalWrite(CE_PIN, LOW);
  digitalWrite(CSN_PIN, HIGH);
  
  // Initialize SPI
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(10000000);
  SPI.setBitOrder(MSBFIRST);
  
  // Initialize display
  u8g2_for_adafruit_gfx.begin(display);
  
  // Initialize history buffer
  memset(signalHistory, 0, sizeof(signalHistory));
  
  // Show startup screen for 3 seconds
  showStartupScreen();
  showStartupUntil = millis() + 3000;
  
  // Initialize NRF24
  delay(100);
  powerUp();
  configureReceiver();
  setChannel(currentChannel);
  
  Serial.println("Analyzer ready, monitoring channel 0");
}

// Main loop
void analyzerLoop() {
  // Show startup screen if still in startup period
  if (millis() < showStartupUntil) {
    return;
  }
  
  // Check for button presses
  checkButtons();
  
  // Scan current channel
  performChannelScan();
  
  // Update display - limit refresh rate
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 50) { // 20 FPS
    drawChannelGraph();
    lastDisplayUpdate = millis();
  }
}