// Project Starbeam V2 - CC1101 Radio Module
// Ported from V1 for modular architecture

#ifndef CC1101_RADIO_H
#define CC1101_RADIO_H

#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <ELECHOUSE_CC1101_SRC_DRV2.h>
#include "../config.h"

// Forward declare display for scan function
class Adafruit_SSD1306;

class CC1101Radio {
public:
    // Initialization
    static void init();          // Initialize CC1101 #1
    static void init2();         // Initialize CC1101 #2
    static bool checkConnection();
    static bool checkConnection2();

    // Operations
    static void scan(float startFreq, float endFreq);  // Frequency scanning
    static void toggleJammingMode();
    static void stopJamming();

    // Configuration
    static void setMhz(float freq);
    static void setModulation(int mode);
    static void setDeviation(float deviation);
    static void setChannel(int channel);
    static void setChsp(float spacing);
    static void setRxBw(float bandwidth);
    static void setDRate(float datarate);
    static void setPa(int power);
    static void setSyncMode(int mode);
    static void setSyncWord(int high, int low);

    // Status
    static float getRssi();
    static int getLqi();
    static void printStatus();

    // Get CC1101 instances for direct access if needed
    static ELECHOUSE_CC1101& getCC1() { return CC1; }
    static ELECHOUSE_CC1101_2& getCC2() { return CC2; }

    // RF Signal API (for web interface)
    static String rfScanJSON(float startFreq, float endFreq, float step);
    static void rfCapture(float freq, int durationMs);
    static void rfReplay();
    static String rfCaptureInfoJSON();
    static void rfClearCapture();
    
    struct RFCapture {
        unsigned long timings[512];   // microseconds tussen edges
        uint8_t levels[512];          // signaalniveau per edge
        int count;
        float freq;
        unsigned long duration_ms;
        bool valid;
    };
    

private:
    static RFCapture lastCapture;
    static ELECHOUSE_CC1101 CC1;
    static ELECHOUSE_CC1101_2 CC2;
    static bool jammingMode;
    static byte sendBuffer[64];
};

#endif // CC1101_RADIO_H