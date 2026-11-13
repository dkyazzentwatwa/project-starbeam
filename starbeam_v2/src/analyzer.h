/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#ifndef ANALYZER_H
#define ANALYZER_H

#include <SPI.h>
#include "esp_bt.h"
#include "esp_wifi.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>

void analyzerSetup();
void analyzerLoop();
int getAnalyzerChannel();
int getAnalyzerSignal();

#endif
