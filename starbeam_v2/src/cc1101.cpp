// Project Starbeam V2 - CC1101 Radio Module Implementation
// Ported from V1

#include "cc1101.h"
#include "display.h"
#include "settings.h"
#include <Adafruit_SSD1306.h>

// Static member initialization
ELECHOUSE_CC1101 CC1101Radio::CC1;
ELECHOUSE_CC1101_2 CC1101Radio::CC2;
bool CC1101Radio::jammingMode = false;
byte CC1101Radio::sendBuffer[64] = {0};
CC1101Radio::RFCapture CC1101Radio::lastCapture = {0};

// Initialize CC1101 #1 with default settings
void CC1101Radio::init() {
    // Set custom SPI pins for CC1101 #1
    CC1.setSpiPin(CC1101_1_SCK, CC1101_1_MISO, CC1101_1_MOSI, CC1101_1_SS);
    CC1.setGDO(CC1101_1_GDO0, CC1101_1_GDO2);

    // Initialize
    CC1.Init();
    CC1.setGDO0(CC1101_1_GDO0);

    // Configure modulation and frequency
    CC1.setCCMode(1);           // Internal transmission mode
    CC1.setModulation(2);       // ASK/OOK
    CC1.setMHZ(433.92);         // Default frequency
    CC1.setDeviation(47.60);    // Frequency deviation
    CC1.setChannel(0);          // Channel 0
    CC1.setChsp(199.95);        // Channel spacing
    CC1.setRxBW(812.50);        // Receive bandwidth
    CC1.setDRate(9.6);          // Data rate
    CC1.setPA(10);              // Max TX power
    CC1.setSyncMode(2);         // 16/16 sync word bits
    CC1.setSyncWord(211, 145);  // Sync word
    CC1.setAdrChk(0);           // No address check
    CC1.setAddr(0);             // Address 0
    CC1.setWhiteData(0);        // Whitening off
    CC1.setPktFormat(0);        // Normal mode
    CC1.setLengthConfig(1);     // Variable packet length
    CC1.setPacketLength(0);     // Max packet length
    CC1.setCrc(0);              // CRC disabled
    CC1.setCRC_AF(0);           // CRC autoflush disabled
    CC1.setDcFilterOff(0);      // DC filter enabled
    CC1.setManchester(0);       // Manchester disabled
    CC1.setFEC(0);              // FEC disabled
    CC1.setPRE(0);              // 2 preamble bytes
    CC1.setPQT(0);              // PQT 0
    CC1.setAppendStatus(0);     // Status bytes disabled

    Serial.println("CC1101 #1 initialized");
}

// Initialize CC1101 #2 with default settings
void CC1101Radio::init2() {
    // Set custom SPI pins for CC1101 #2
    CC2.setSpiPin(CC1101_2_SCK, CC1101_2_MISO, CC1101_2_MOSI, CC1101_2_SS);
    CC2.setGDO(CC1101_2_GDO0, CC1101_2_GDO2);

    // Initialize
    CC2.Init();
    CC2.setGDO0(CC1101_2_GDO0);

    // Configure modulation and frequency (same as CC1)
    CC2.setCCMode(1);
    CC2.setModulation(2);
    CC2.setMHZ(433.92);
    CC2.setDeviation(47.60);
    CC2.setChannel(0);
    CC2.setChsp(199.95);
    CC2.setRxBW(812.50);
    CC2.setDRate(9.6);
    CC2.setPA(10);
    CC2.setSyncMode(2);
    CC2.setSyncWord(211, 145);
    CC2.setAdrChk(0);
    CC2.setAddr(0);
    CC2.setWhiteData(0);
    CC2.setPktFormat(0);
    CC2.setLengthConfig(1);
    CC2.setPacketLength(0);
    CC2.setCrc(0);
    CC2.setCRC_AF(0);
    CC2.setDcFilterOff(0);
    CC2.setManchester(0);
    CC2.setFEC(0);
    CC2.setPRE(0);
    CC2.setPQT(0);
    CC2.setAppendStatus(0);

    Serial.println("CC1101 #2 initialized");
}

bool CC1101Radio::checkConnection() {
    return CC1.getCC1101();
}

bool CC1101Radio::checkConnection2() {
    return CC2.getCC1101();
}

// Frequency scanning with RSSI display
void CC1101Radio::scan(float startFreq, float endFreq) {
    Adafruit_SSD1306& oled = Display::getOled();

    struct SignalInfo {
        float frequency;
        float rssi;
        unsigned long timestamp;
    };

    SignalInfo foundSignals[MAX_SIGNALS];
    int signalCount = 0;

    Serial.printf("Scanning %.2f MHz to %.2f MHz\n", startFreq, endFreq);

    // Initialize for scanning
    CC1.Init();
    CC1.setRxBW(58);
    CC1.SetRx();

    float freq = startFreq;
    unsigned long displayUpdateTime = 0;
    const unsigned long DISPLAY_HOLD_TIME = 3000;

    while (true) {
        // Check for exit (SELECT button)
        if (digitalRead(BUTTON_SELECT) == LOW) {
            delay(100);
            if (digitalRead(BUTTON_SELECT) == LOW) {
                break;
            }
        }

        CC1.setMHZ(freq);
        float rssi = CC1.getRssi();

        // Update display
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setTextColor(SSD1306_WHITE);
        oled.setCursor(0, 0);
        oled.print("Scan: ");
        oled.print(freq, 2);
        oled.println(" MHz");

        // Check for strong signals
        if (rssi > -75) {
            bool signalExists = false;
            for (int i = 0; i < signalCount; i++) {
                if (abs(foundSignals[i].frequency - freq) < 0.05) {
                    signalExists = true;
                    foundSignals[i].rssi = rssi;
                    foundSignals[i].timestamp = millis();
                    break;
                }
            }

            if (!signalExists && signalCount < MAX_SIGNALS) {
                foundSignals[signalCount].frequency = freq;
                foundSignals[signalCount].rssi = rssi;
                foundSignals[signalCount].timestamp = millis();
                signalCount++;
            }

            Serial.printf("Signal: %.2f MHz, RSSI: %.1f dBm\n", freq, rssi);
        }

        // Update display with found signals
        if (millis() - displayUpdateTime > 500) {
            // Remove old signals
            unsigned long currentTime = millis();
            for (int i = 0; i < signalCount; i++) {
                if (currentTime - foundSignals[i].timestamp > DISPLAY_HOLD_TIME) {
                    for (int j = i; j < signalCount - 1; j++) {
                        foundSignals[j] = foundSignals[j + 1];
                    }
                    signalCount--;
                    i--;
                }
            }

            // Display found signals
            if (signalCount > 0) {
                oled.setCursor(0, 10);
                oled.println("Signals:");
                for (int i = 0; i < signalCount; i++) {
                    oled.setCursor(0, 20 + (i * 10));
                    oled.print(foundSignals[i].frequency, 2);
                    oled.print(" MHz ");
                    oled.print(foundSignals[i].rssi, 1);
                    oled.println(" dBm");
                }
            }

            oled.display();
            displayUpdateTime = millis();
        }

        // Increment frequency
        freq += 0.05;
        if (freq > endFreq) {
            freq = startFreq;
        }

        delay(10);
    }
}

void CC1101Radio::toggleJammingMode() {
    if (jammingMode) {
        jammingMode = false;
        Serial.println("Jamming disabled");
    } else {
        jammingMode = true;
        randomSeed(analogRead(0));
        for (int i = 0; i < 60; i++) {
            sendBuffer[i] = (byte)random(255);
        }
        CC1.SendData(sendBuffer, 60);
        CC2.SendData(sendBuffer, 60);
        Serial.println("Jamming enabled");
    }
}

void CC1101Radio::stopJamming() {
    jammingMode = false;
}

// Configuration functions
void CC1101Radio::setMhz(float freq) {
    CC1.setMHZ(freq);
    CC2.setMHZ(freq);
    Serial.printf("Frequency: %.2f MHz\n", freq);
}

void CC1101Radio::setModulation(int mode) {
    CC1.setModulation(mode);
    CC2.setModulation(mode);
    const char* modes[] = {"2-FSK", "GFSK", "ASK/OOK", "4-FSK", "MSK"};
    Serial.printf("Modulation: %s\n", modes[mode]);
}

void CC1101Radio::setDeviation(float deviation) {
    CC1.setDeviation(deviation);
    CC2.setDeviation(deviation);
    Serial.printf("Deviation: %.2f kHz\n", deviation);
}

void CC1101Radio::setChannel(int channel) {
    CC1.setChannel(channel);
    CC2.setChannel(channel);
    Serial.printf("Channel: %d\n", channel);
}

void CC1101Radio::setChsp(float spacing) {
    CC1.setChsp(spacing);
    CC2.setChsp(spacing);
    Serial.printf("Channel spacing: %.2f kHz\n", spacing);
}

void CC1101Radio::setRxBw(float bandwidth) {
    CC1.setRxBW(bandwidth);
    CC2.setRxBW(bandwidth);
    Serial.printf("RX bandwidth: %.2f kHz\n", bandwidth);
}

void CC1101Radio::setDRate(float datarate) {
    CC1.setDRate(datarate);
    CC2.setDRate(datarate);
    Serial.printf("Data rate: %.2f kBaud\n", datarate);
}

void CC1101Radio::setPa(int power) {
    CC1.setPA(power);
    CC2.setPA(power);
    Serial.printf("TX power: %d dBm\n", power);
}

void CC1101Radio::setSyncMode(int mode) {
    CC1.setSyncMode(mode);
    CC2.setSyncMode(mode);
    Serial.printf("Sync mode: %d\n", mode);
}

void CC1101Radio::setSyncWord(int high, int low) {
    CC1.setSyncWord(low, high);
    CC2.setSyncWord(low, high);
    Serial.printf("Sync word: high=%d, low=%d\n", high, low);
}

float CC1101Radio::getRssi() {
    return CC1.getRssi();
}

int CC1101Radio::getLqi() {
    return CC1.getLqi();
}

void CC1101Radio::printStatus() {
    Serial.printf("RSSI: %.1f dBm, LQI: %d\n", CC1.getRssi(), CC1.getLqi());
}

// ============================================================================
// RF SIGNAL API - Frequency scan, raw capture, replay
// ============================================================================

String CC1101Radio::rfScanJSON(float startFreq, float endFreq, float step) {
    // Validatie
    if (startFreq < 300 || endFreq > 928 || endFreq <= startFreq || step <= 0) {
        return "{\"error\":\"Invalid range (use 300-928 MHz, start<end, step>0)\"}";
    }
    if ((endFreq - startFreq) / step > 500) {
        return "{\"error\":\"Too many steps (max 500)\"}";
    }
    
    // Bewaar huidige frequentie
    float savedFreq = Settings::freq();
    
    // Zet CC1 in RX
    CC1.setMHZ(startFreq);
    CC1.SetRx();
    delay(10);
    
    String json = "{\"start\":" + String(startFreq, 2) + ",";
    json += "\"end\":" + String(endFreq, 2) + ",";
    json += "\"step\":" + String(step, 3) + ",";
    json += "\"signals\":[";
    
    bool first = true;
    float peakFreq = 0;
    float peakRssi = -999;
    
    for (float f = startFreq; f <= endFreq; f += step) {
        CC1.setMHZ(f);
        delay(3);  // Settle tijd voor de PLL
        float rssi = CC1.getRssi();
        
        // Alleen interessante signalen rapporteren (boven -85 dBm)
        if (rssi > -85) {
            if (!first) json += ",";
            json += "{\"freq\":" + String(f, 3);
            json += ",\"rssi\":" + String(rssi, 1) + "}";
            first = false;
        }
        
        // Hou het sterkste signaal bij
        if (rssi > peakRssi) {
            peakRssi = rssi;
            peakFreq = f;
        }
        
        yield();  // WDT reset
    }
    
    json += "],";
    json += "\"peak_freq\":" + String(peakFreq, 3) + ",";
    json += "\"peak_rssi\":" + String(peakRssi, 1);
    json += "}";
    
    // Herstel
    CC1.setMHZ(savedFreq);
    
    return json;
}

void CC1101Radio::rfCapture(float freq, int durationMs) {
    lastCapture.count = 0;
    lastCapture.valid = false;
    lastCapture.freq = freq;
    lastCapture.duration_ms = durationMs;
    
    // Configureer CC1101 voor raw RX (OOK/ASK, async serial mode)
    CC1.setMHZ(freq);
    CC1.setModulation(2);           // ASK/OOK
    CC1.setDRate(9.6);              // Niet heel kritisch in async mode
    CC1.setCCMode(0);               // Async serial mode (geen packet handling)
    CC1.SetRx();
    delay(20);
    
    // Wacht op eerste edge (rising of falling) - tot 500ms
    unsigned long startWait = millis();
    int lastLevel = digitalRead(CC1101_1_GDO0);
    while (millis() - startWait < 500) {
        int level = digitalRead(CC1101_1_GDO0);
        if (level != lastLevel) break;
        delayMicroseconds(50);
        yield();
    }
    
    // Als er niets gebeurt, is er geen signaal
    if (millis() - startWait >= 500) {
        Serial.println("[RF] No signal detected during 500ms wait");
        lastCapture.valid = false;
        return;
    }
    
    // Capture loop
    unsigned long captureStart = micros();
    unsigned long lastEdge = captureStart;
    int prevLevel = digitalRead(CC1101_1_GDO0);
    
    while (millis() - captureStart / 1000 < durationMs && lastCapture.count < 511) {
        int level = digitalRead(CC1101_1_GDO0);
        
        if (level != prevLevel) {
            unsigned long now = micros();
            lastCapture.timings[lastCapture.count] = now - lastEdge;
            lastCapture.levels[lastCapture.count] = prevLevel;
            lastCapture.count++;
            lastEdge = now;
            prevLevel = level;
        }
        yield();
    }
    
    lastCapture.valid = lastCapture.count > 4;  // Minimaal 5 edges om zinvol te zijn
    Serial.printf("[RF] Captured %d edges at %.2f MHz (%lu ms)\n",
                  lastCapture.count, freq, durationMs);
}

void CC1101Radio::rfReplay() {
    if (!lastCapture.valid || lastCapture.count == 0) {
        Serial.println("[RF] No valid capture to replay");
        return;
    }
    
    Serial.printf("[RF] Replaying %d edges at %.2f MHz\n",
                  lastCapture.count, lastCapture.freq);
    
    // Configureer CC1101 voor TX in async serial mode
    CC1.setMHZ(lastCapture.freq);
    CC1.setModulation(2);           // ASK/OOK
    CC1.setCCMode(0);               // Async serial mode
    CC1.setPA(10);                  // Max power
    
    // Zet GDO0 als OUTPUT voor bit-banging
    pinMode(CC1101_1_GDO0, OUTPUT);
    digitalWrite(CC1101_1_GDO0, LOW);
    
    // Ga naar TX
    CC1.SetTx();
    delay(10);
    
    // Speel de timings af
    for (int i = 0; i < lastCapture.count; i++) {
        digitalWrite(CC1101_1_GDO0, lastCapture.levels[i]);
        delayMicroseconds(lastCapture.timings[i]);
    }
    
    // Stop TX
    digitalWrite(CC1101_1_GDO0, LOW);
    CC1.setSidle();
    
    // Zet GDO0 terug als INPUT
    pinMode(CC1101_1_GDO0, INPUT);
    
    Serial.println("[RF] Replay finished");
}

String CC1101Radio::rfCaptureInfoJSON() {
    if (!lastCapture.valid) {
        return "{\"valid\":false}";
    }
    
    String json = "{\"valid\":true,";
    json += "\"freq\":" + String(lastCapture.freq, 2) + ",";
    json += "\"count\":" + String(lastCapture.count) + ",";
    json += "\"duration_ms\":" + String(lastCapture.duration_ms) + ",";
    
    // Bereken totale duur van het signaal
    unsigned long totalUs = 0;
    for (int i = 0; i < lastCapture.count; i++) {
        totalUs += lastCapture.timings[i];
    }
    json += "\"signal_duration_us\":" + String(totalUs) + ",";
    
    // Toon eerste 32 edges als preview
    json += "\"preview\":[";
    int previewCount = min(lastCapture.count, 32);
    for (int i = 0; i < previewCount; i++) {
        if (i > 0) json += ",";
        json += "{\"t\":" + String(lastCapture.timings[i]);
        json += ",\"l\":" + String(lastCapture.levels[i]) + "}";
    }
    json += "]}";
    
    return json;
}

void CC1101Radio::rfClearCapture() {
    lastCapture.count = 0;
    lastCapture.valid = false;
    Serial.println("[RF] Capture cleared");
}