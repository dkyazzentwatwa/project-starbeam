/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include <Arduino.h>
#include "analyzer.h"
#include "display.h"
#include "config.h"

// Get display from Display module
#define u8g2_for_adafruit_gfx Display::getU8g2()

// NRF24L01 Register definitions
#define NRF24_CONFIG      0x00
#define NRF24_EN_AA       0x01
#define NRF24_EN_RXADDR   0x02
#define NRF24_SETUP_AW    0x03
#define NRF24_RF_CH       0x05
#define NRF24_RF_SETUP    0x06
#define NRF24_STATUS      0x07
#define NRF24_RPD         0x09

// Analyzer settings
#define MAX_CHANNELS      128       // Total available channels
#define HISTORY_SIZE      120       // Number of historical values to keep (for graph)
#define SIGNAL_MAX        10        // Maximum signal strength value
#define SCAN_INTERVAL     10        // Time between scans in milliseconds

// SPI pins for analyzer (uses VSPI)
#define ANALYZER_CE       NRF24_1_CE
#define ANALYZER_CSN      NRF24_1_CS
#define ANALYZER_SCK      18
#define ANALYZER_MISO     19
#define ANALYZER_MOSI     23
#define ANALYZER_SS       17

// Global variables
static uint8_t currentChannel = 0;
static uint8_t signalHistory[HISTORY_SIZE];
static uint8_t historyIndex = 0;
static uint8_t maxSignal = 0;

static unsigned long lastButtonCheck = 0;
static unsigned long lastChannelScan = 0;
static unsigned long showStartupUntil = 0;

// SPI communication functions
static uint8_t readRegister(uint8_t reg) {
    digitalWrite(ANALYZER_CSN, LOW);
    SPI.transfer(reg & 0x1F);
    uint8_t result = SPI.transfer(0x00);
    digitalWrite(ANALYZER_CSN, HIGH);
    return result;
}

static void writeRegister(uint8_t reg, uint8_t value) {
    digitalWrite(ANALYZER_CSN, LOW);
    SPI.transfer((reg & 0x1F) | 0x20);
    SPI.transfer(value);
    digitalWrite(ANALYZER_CSN, HIGH);
}

// NRF24 control functions
static void setChannel(uint8_t channel) {
    writeRegister(NRF24_RF_CH, channel);
    currentChannel = channel;
}

static void powerUp() {
    uint8_t config = readRegister(NRF24_CONFIG);
    writeRegister(NRF24_CONFIG, config | 0x02);
    delay(2);
}

static void configureReceiver() {
    writeRegister(NRF24_CONFIG, 0x03);
    writeRegister(NRF24_EN_AA, 0x00);
    writeRegister(NRF24_SETUP_AW, 0x01);
    writeRegister(NRF24_RF_SETUP, 0x0F);
    writeRegister(NRF24_EN_RXADDR, 0x01);
}

static void startReceiving() {
    digitalWrite(ANALYZER_CE, HIGH);
}

static void stopReceiving() {
    digitalWrite(ANALYZER_CE, LOW);
}

static uint8_t detectSignalStrength() {
    uint8_t strength = 0;

    for (int i = 0; i < 3; i++) {
        startReceiving();
        delayMicroseconds(200);

        if (readRegister(NRF24_RPD) & 0x01) {
            strength += 2;
        }

        stopReceiving();
        delayMicroseconds(50);
    }

    if (strength > SIGNAL_MAX) {
        strength = SIGNAL_MAX;
    }

    return strength;
}

static void addSignalToHistory(uint8_t signal) {
    signalHistory[historyIndex] = signal;

    if (signal > maxSignal) {
        maxSignal = signal;
    } else {
        if (historyIndex == 0) {
            maxSignal = 0;
            for (int i = 0; i < HISTORY_SIZE; i++) {
                if (signalHistory[i] > maxSignal) {
                    maxSignal = signalHistory[i];
                }
            }
        }
    }

    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
}

static void performChannelScan() {
    if (millis() - lastChannelScan < SCAN_INTERVAL) {
        return;
    }

    lastChannelScan = millis();
    setChannel(currentChannel);
    uint8_t signal = detectSignalStrength();
    addSignalToHistory(signal);
}

static void checkButtons() {
    if (millis() - lastButtonCheck < 100) {
        return;
    }
    lastButtonCheck = millis();

    bool upPressed = (digitalRead(BUTTON_UP) == LOW);
    bool downPressed = (digitalRead(BUTTON_DOWN) == LOW);

    if (upPressed) {
        if (currentChannel < MAX_CHANNELS - 1) {
            currentChannel++;
        } else {
            currentChannel = 0;
        }

        memset(signalHistory, 0, sizeof(signalHistory));
        maxSignal = 0;
        historyIndex = 0;
        setChannel(currentChannel);
        delay(100);
    }

    if (downPressed) {
        if (currentChannel > 0) {
            currentChannel--;
        } else {
            currentChannel = MAX_CHANNELS - 1;
        }

        memset(signalHistory, 0, sizeof(signalHistory));
        maxSignal = 0;
        historyIndex = 0;
        setChannel(currentChannel);
        delay(100);
    }
}

static void drawChannelGraph() {
    Adafruit_SSD1306& display = Display::getOled();
    display.clearDisplay();

    u8g2_for_adafruit_gfx.setFont(u8g2_font_ncenB10_tr);
    u8g2_for_adafruit_gfx.setCursor(0, 10);
    u8g2_for_adafruit_gfx.print("Channel: ");
    u8g2_for_adafruit_gfx.print(currentChannel);

    if (currentChannel == 1 || currentChannel == 6 || currentChannel == 11) {
        u8g2_for_adafruit_gfx.setCursor(90, 10);
        u8g2_for_adafruit_gfx.print("WiFi");
    }

    display.drawFastHLine(0, 12, SCREEN_WIDTH, SSD1306_WHITE);

    u8g2_for_adafruit_gfx.setFont(u8g2_font_profont10_mr);
    u8g2_for_adafruit_gfx.setCursor(0, 22);
    u8g2_for_adafruit_gfx.print("Max");
    u8g2_for_adafruit_gfx.setCursor(0, SCREEN_HEIGHT - 2);
    u8g2_for_adafruit_gfx.print("Min");

    display.drawFastVLine(20, 13, SCREEN_HEIGHT - 13, SSD1306_WHITE);

    float xScale = (float)(SCREEN_WIDTH - 25) / HISTORY_SIZE;
    float yScale = (float)(SCREEN_HEIGHT - 15) / SIGNAL_MAX;

    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        int idx1 = (historyIndex + i) % HISTORY_SIZE;
        int idx2 = (historyIndex + i + 1) % HISTORY_SIZE;

        int x1 = 22 + i * xScale;
        int x2 = 22 + (i + 1) * xScale;

        int y1 = SCREEN_HEIGHT - 2 - (signalHistory[idx1] * yScale);
        int y2 = SCREEN_HEIGHT - 2 - (signalHistory[idx2] * yScale);

        display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }

    uint8_t currentSignal = signalHistory[(historyIndex == 0) ? HISTORY_SIZE - 1 : historyIndex - 1];
    int barHeight = currentSignal * yScale;
    display.fillRect(SCREEN_WIDTH - 10, SCREEN_HEIGHT - 2 - barHeight, 8, barHeight, SSD1306_WHITE);

    u8g2_for_adafruit_gfx.setCursor(SCREEN_WIDTH - 40, 22);
    u8g2_for_adafruit_gfx.print("Sig:");
    u8g2_for_adafruit_gfx.print(currentSignal);

    u8g2_for_adafruit_gfx.setCursor(0, SCREEN_HEIGHT);
    u8g2_for_adafruit_gfx.print("CH- [DOWN]    [UP] CH+");

    display.display();
}

static void showStartupScreen() {
    Adafruit_SSD1306& display = Display::getOled();
    display.clearDisplay();

    u8g2_for_adafruit_gfx.setFont(u8g2_font_ncenB14_tr);
    u8g2_for_adafruit_gfx.setCursor(5, 20);
    u8g2_for_adafruit_gfx.print("NRF24 Scanner");

    u8g2_for_adafruit_gfx.setFont(u8g2_font_profont10_mr);
    u8g2_for_adafruit_gfx.setCursor(20, 35);
    u8g2_for_adafruit_gfx.print("Single Channel Mode");

    u8g2_for_adafruit_gfx.setCursor(10, 50);
    u8g2_for_adafruit_gfx.print("UP/DOWN: Change channel");

    display.display();
}

void analyzerSetup() {
    Serial.println("NRF24 WiFi Single Channel Analyzer");

    // esp_bt_controller_deinit();
    // esp_wifi_stop();
    // esp_wifi_deinit();

    pinMode(ANALYZER_CE, OUTPUT);
    pinMode(ANALYZER_CSN, OUTPUT);

    // FIXED: Changed INPUT_PULLUP to INPUT to prevent GPIO errors on pins 34/39
    pinMode(BUTTON_UP, INPUT);
    pinMode(BUTTON_DOWN, INPUT);
    //old:
    //pinMode(BUTTON_UP, INPUT_PULLUP);
    //pinMode(BUTTON_DOWN, INPUT_PULLUP);

    digitalWrite(ANALYZER_CE, LOW);
    digitalWrite(ANALYZER_CSN, HIGH);

    SPI.begin(ANALYZER_SCK, ANALYZER_MISO, ANALYZER_MOSI, ANALYZER_SS);
    SPI.setDataMode(SPI_MODE0);
    SPI.setFrequency(10000000);
    SPI.setBitOrder(MSBFIRST);

    memset(signalHistory, 0, sizeof(signalHistory));

    showStartupScreen();
    showStartupUntil = millis() + 3000;

    delay(100);
    powerUp();
    configureReceiver();
    setChannel(currentChannel);

    Serial.println("Analyzer ready, monitoring channel 0");
}

void analyzerLoop() {
    if (millis() < showStartupUntil) {
        return;
    }

    checkButtons();
    performChannelScan();

    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 50) {
        drawChannelGraph();
        lastDisplayUpdate = millis();
    }
}

int getAnalyzerChannel() {
    return currentChannel;
}

int getAnalyzerSignal() {
    // Pak de meest recente waarde uit de geschiedenis
    int idx = (historyIndex == 0) ? HISTORY_SIZE - 1 : historyIndex - 1;
    return signalHistory[idx];
}