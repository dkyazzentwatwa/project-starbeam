// settings.cpp - EEPROM-backed settings store

#include <EEPROM.h>
#include "settings.h"

Settings::Data Settings::data;
bool Settings::loaded = false;

// Add a variable to track the state
static bool _wifiStaEnabled = DEFAULT_WIFI_STA_ENABLED;

bool Settings::wifiStaEnabled() {
    return _wifiStaEnabled;
}

void Settings::setWifiStaEnabled(bool enabled) {
    _wifiStaEnabled = enabled;
    // TODO: Save to EEPROM here (using EEPROM.put) if you want it to persist across reboots
}

static void setDefaults(Settings::Data& d) {
    d.magic = Settings::MAGIC;
    d.version = Settings::VERSION;
    d.echo_enabled = 1;
    d.verbose = 0;
    d.ui_theme = 0;
    d.cc1101_freq = 433.92f;
    d.boot_count = 0;
    for (uint8_t& b : d.reserved) b = 0;
}

bool Settings::readBlob() {
    for (size_t i = 0; i < sizeof(Data); ++i) {
        ((uint8_t*)&data)[i] = EEPROM.read(SETTINGS_EEPROM_OFFSET + i);
    }
    return data.magic == MAGIC && data.version == VERSION;
}

void Settings::writeBlob() {
    for (size_t i = 0; i < sizeof(Data); ++i) {
        EEPROM.write(SETTINGS_EEPROM_OFFSET + i, ((uint8_t*)&data)[i]);
    }
    EEPROM.commit();
}

void Settings::init() {
    if (loaded) return;
    if (!readBlob()) {
        setDefaults(data);
        writeBlob();
        Serial.println(F("[settings] EEPROM blank or invalid -> defaults written"));
    } else {
        Serial.printf("[settings] loaded v%u (boot #%u)\n",
                      (unsigned)data.version, (unsigned)data.boot_count);
    }
    data.boot_count++;
    writeBlob();
    loaded = true;
}

void Settings::save() {
    writeBlob();
}

void Settings::resetDefaults() {
    setDefaults(data);
    writeBlob();
    Serial.println(F("[settings] reset to defaults"));
}

void Settings::setFreq(float f) {
    if (f == data.cc1101_freq) return;
    data.cc1101_freq = f;
    save();
}

void Settings::setEcho(bool v) {
    uint8_t nv = v ? 1 : 0;
    if (nv == data.echo_enabled) return;
    data.echo_enabled = nv;
    save();
}

void Settings::setVerbose(bool v) {
    uint8_t nv = v ? 1 : 0;
    if (nv == data.verbose) return;
    data.verbose = nv;
    save();
}

void Settings::setTheme(uint8_t t) {
    if (t == data.ui_theme) return;
    data.ui_theme = t;
    save();
}
