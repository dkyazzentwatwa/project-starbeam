// settings.h - Persistent settings stored in EEPROM
// Lives at SETTINGS_EEPROM_OFFSET so it does not collide with Recording (0..511).

#ifndef STARBEAM_SETTINGS_H
#define STARBEAM_SETTINGS_H

#include <Arduino.h>
#include "../config.h"

class Settings {
public:
    static const uint32_t MAGIC = 0x53544231;  // "STB1"
    static const uint8_t  VERSION = 1;

    struct Data {
        uint32_t magic;
        uint8_t  version;
        uint8_t  echo_enabled;     // serial echo
        uint8_t  verbose;          // verbose logging
        uint8_t  ui_theme;         // 0 = light, 1 = inverted
        float    cc1101_freq;      // last CC1101 frequency (MHz)
        uint16_t boot_count;
        uint8_t  reserved[44];
    };

    static void   init();              // load + apply, increments boot_count
    static void   save();
    static void   resetDefaults();

    static float  freq()         { return data.cc1101_freq; }
    static void   setFreq(float f);

    static bool   echo()         { return data.echo_enabled; }
    static void   setEcho(bool v);

    static bool   verbose()      { return data.verbose; }
    static void   setVerbose(bool v);

    static uint8_t theme()       { return data.ui_theme; }
    static void    setTheme(uint8_t t);

    static uint16_t bootCount()  { return data.boot_count; }

    static const Data& raw() { return data; }

    static bool wifiStaEnabled();
    static void setWifiStaEnabled(bool enabled);
    
private:
    static Data data;
    static bool loaded;
    static void writeBlob();
    static bool readBlob();
};

#endif
