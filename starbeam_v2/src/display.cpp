// display.cpp - Display Module Implementation for Project Starbeam V2
// Phase 3 cleanup: redraw caching + cleaner OLED layout.
//
// Layout philosophy:
//   - 128x64 SSD1306 is tight; every pixel counts.
//   - Title bar (12px) inverted, with a thin separator underneath.
//   - 3-line body at small font, optional 4th hint line.
//   - Menu shows 3 entries with a chevron + thin scrollbar on the right.

#include "display.h"
#include "terminal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Static member initialization
Adafruit_SSD1306 Display::oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
U8G2_FOR_ADAFRUIT_GFX Display::u8g2;

// Menu labels array (must match MenuItem enum order in types.h)
const char* Display::menuLabels[NUM_MENU_ITEMS] = {
    "BT Jammer",
    "Drone Jammer",
    "Wifi Jammer",
    "CC1 Jammer",
    "CC1101 Scan",
    "NRF Scan",
    "WiFi Scanner",
    "WiFi Heatmap",
    "BLE Scanner",
    "Flock Detector",
    "Captive Portal",
    "Pkt Monitor",
    "Web Server ON",
    "Web Server OFF",
    "Web Status",
    "Deauth Target",
    "Deauth All",
    "Beacon Flood",
    "Probe Flood",
    "PMKID Capture",
    "NRF Test",
    "CC1101 Test",
    "Test HSPI",
    "WiFi Mode",  
    "CC Single",
    "CC2 Single",
    "Rec Raw",
    "Play Raw",
    "Show Raw",
    "Show Buffer",
    "Get RSSI",
    "Flush Buffer",
    "Stop CC1101",
    "Reset CC1101",
    "434.40 MHz",
    "434.30 MHz",
    "434.00 MHz",
    "433.90 MHz",
    "Settings",
    "Help"
};

// ---------------------------------------------------------------------------
// Redraw cache (avoids hammering I2C with identical frames in tight loops)
// ---------------------------------------------------------------------------
namespace {
    String last_title, last_a, last_b, last_c;
    bool   last_kind_info = false;
    int    last_menu_sel = -1;
    int    last_menu_first = -1;
}

void Display::init() {
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Halt if display init fails
    }
    u8g2.begin(oled);
    oled.clearDisplay();
    oled.display();
}

// ---------------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------------
static void drawHeaderBar(Adafruit_SSD1306& oled, const char* title) {
    oled.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setTextSize(1);
    oled.setCursor(3, 2);
    oled.print(title);
    // small accent on the right side
    oled.drawFastVLine(SCREEN_WIDTH - 1, 0, 12, SSD1306_BLACK);
    oled.setTextColor(SSD1306_WHITE);
}

void Display::drawMenu(MenuItem selectedMenuItem, int firstVisibleMenuItem) {
    // Cache: skip redraw if nothing changed.
    if (!last_kind_info &&
        last_menu_sel   == (int)selectedMenuItem &&
        last_menu_first == firstVisibleMenuItem) {
        return;
    }
    last_kind_info = false;
    last_menu_sel = (int)selectedMenuItem;
    last_menu_first = firstVisibleMenuItem;

    oled.clearDisplay();
    drawHeaderBar(oled, "Starbeam V2");

    // Body: 3 menu items, font ~10px tall.
    const int kRows = 3;
    const int kRowH = 14;
    const int yStart = 16;
    const int sbX = SCREEN_WIDTH - 4;   // scrollbar x

    oled.setTextSize(1);
    for (int i = 0; i < kRows; i++) {
        int menuIndex = (firstVisibleMenuItem + i) % NUM_MENU_ITEMS;
        int y = yStart + i * kRowH;
        bool sel = (selectedMenuItem == menuIndex);

        if (sel) {
            // Filled highlight with rounded ends
            oled.fillRoundRect(0, y - 2, sbX - 2, kRowH - 1, 2, SSD1306_WHITE);
            oled.setTextColor(SSD1306_BLACK);
            oled.setCursor(8, y);
            oled.print(menuLabels[menuIndex]);
            // Chevron at left edge
            oled.fillTriangle(2, y - 1, 2, y + 7, 6, y + 3, SSD1306_BLACK);
        } else {
            oled.setTextColor(SSD1306_WHITE);
            oled.setCursor(8, y);
            oled.print(menuLabels[menuIndex]);
        }
    }

    // Scrollbar: track + thumb
    oled.drawFastVLine(sbX, yStart - 2, kRows * kRowH, SSD1306_WHITE);
    int trackH = kRows * kRowH;
    int thumbH = max(4, trackH * kRows / NUM_MENU_ITEMS);
    int thumbY = yStart - 2 + (firstVisibleMenuItem * (trackH - thumbH)) /
                              max(1, NUM_MENU_ITEMS - kRows);
    oled.fillRect(sbX + 1, thumbY, 3, thumbH, SSD1306_WHITE);

    oled.display();

    // Echo menu to terminal
    String line1 = "MENU";
    String line2 = String(menuLabels[selectedMenuItem]);
    if (selectedMenuItem > 0) line2 = "> " + line2;
    Terminal::echoToSerial(line1, line2, "[UP/DOWN/SELECT]", "");
}

void Display::drawBorder() {
    oled.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
}

void Display::displayInfo(String title, String info1, String info2, String info3) {
    // Skip redraw if identical to last frame (huge win in spin-wait UIs).
    if (last_kind_info &&
        last_title == title &&
        last_a == info1 && last_b == info2 && last_c == info3) {
        return;
    }
    last_kind_info = true;
    last_title = title; last_a = info1; last_b = info2; last_c = info3;
    last_menu_sel = -1; last_menu_first = -1;

    oled.clearDisplay();
    drawHeaderBar(oled, title.c_str());

    // Body: 3 lines at y = 18, 30, 42; hint area at y = 54 (4th line).
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(2, 18);
    oled.print(info1);
    oled.setCursor(2, 30);
    oled.print(info2);
    oled.setCursor(2, 42);
    oled.print(info3);

    // Subtle bottom hint bar separator
    oled.drawFastHLine(0, 52, SCREEN_WIDTH, SSD1306_WHITE);

    oled.display();
    Terminal::echoToSerial(title, info1, info2, info3);
}

void Display::updateDisplay(const char* message) {
    oled.clearDisplay();
    drawHeaderBar(oled, "Info");
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(2, 18);
    oled.print(message);
    oled.display();
    Terminal::echoToSerial(String(message), "", "", "");
}

void Display::displayTitleScreen() {
    oled.clearDisplay();
    // Big centered title with subtitle
    u8g2.setFont(u8g2_font_adventurer_tr);
    int w = u8g2.getUTF8Width("STARBEAM");
    u8g2.setCursor((SCREEN_WIDTH - w) / 2, 26);
    u8g2.print("STARBEAM");

    u8g2.setFont(u8g2_font_baby_tf);
    const char* sub = "v2  RF intelligence";
    w = u8g2.getUTF8Width(sub);
    u8g2.setCursor((SCREEN_WIDTH - w) / 2, 42);
    u8g2.print(sub);

    // Decorative rule lines
    oled.drawFastHLine(8, 48, SCREEN_WIDTH - 16, SSD1306_WHITE);
    oled.drawFastHLine(24, 52, SCREEN_WIDTH - 48, SSD1306_WHITE);

    oled.display();
}

void Display::displayInfoScreen() {
    oled.clearDisplay();
    drawHeaderBar(oled, "About");
    u8g2.setFont(u8g2_font_baby_tf);
    u8g2.setCursor(2, 24);
    u8g2.print("Project Starbeam V2");
    u8g2.setCursor(2, 34);
    u8g2.print("Multi-band RF tool.");
    u8g2.setCursor(2, 44);
    u8g2.print("Scan. Capture. Replay.");
    u8g2.setCursor(2, 58);
    u8g2.print("Authorized use only.");
    oled.display();
}

// ---------------------------------------------------------------------------
// Animated boot intro — "Project Starbeam / by little hakr"
//
// 4 stages, ~3.0 seconds total:
//   A) starfield warp (radial particles)         ~900ms
//   B) STARBEAM title reveal with glitch noise  ~900ms
//   C) subtitle slide-in "by little hakr"        ~600ms
//   D) scanline sweep + decorative hold          ~600ms
// ---------------------------------------------------------------------------
namespace {

// Tiny xorshift PRNG so we don't pull in randomSeed across boots.
inline uint32_t prand(uint32_t& s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}

void frameDelay(uint16_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// Stage A: radial starfield warp, 24 particles.
void stageStarfield(Adafruit_SSD1306& oled, uint32_t durationMs) {
    const int CX = SCREEN_WIDTH / 2;
    const int CY = SCREEN_HEIGHT / 2;
    const int N  = 24;
    struct P { float r; int angle; uint8_t speed; };
    static P parts[N];
    uint32_t rs = 0xC0FFEEUL;
    for (int i = 0; i < N; i++) {
        parts[i].r = (prand(rs) % 12) + 1;
        parts[i].angle = prand(rs) % 360;
        parts[i].speed = 1 + (prand(rs) % 3);
    }
    uint32_t t0 = millis();
    while (millis() - t0 < durationMs) {
        oled.clearDisplay();
        for (int i = 0; i < N; i++) {
            float rad = parts[i].angle * 0.01745329f;
            int x = CX + (int)(cosf(rad) * parts[i].r);
            int y = CY + (int)(sinf(rad) * parts[i].r);
            // Streak tail: 3 pixels along velocity vector.
            for (int t = 0; t < 3; t++) {
                int tx = CX + (int)(cosf(rad) * (parts[i].r - t));
                int ty = CY + (int)(sinf(rad) * (parts[i].r - t));
                if (tx >= 0 && tx < SCREEN_WIDTH && ty >= 0 && ty < SCREEN_HEIGHT) {
                    oled.drawPixel(tx, ty, SSD1306_WHITE);
                }
            }
            (void)x; (void)y;
            parts[i].r += parts[i].speed * 0.6f;
            if (parts[i].r > 80) {
                parts[i].r = 1;
                parts[i].angle = prand(rs) % 360;
                parts[i].speed = 1 + (prand(rs) % 3);
            }
        }
        // Center pinpoint
        oled.fillRect(CX - 1, CY - 1, 3, 3, SSD1306_WHITE);
        oled.display();
        frameDelay(40);
    }
}

// Stage B: type STARBEAM in big font with a glitch frame between chars.
void stageTitleReveal(Adafruit_SSD1306& oled, U8G2_FOR_ADAFRUIT_GFX& u8g2) {
    const char* title = "STARBEAM";
    const int len = 8;
    uint32_t rs = 0xBADC0DEUL;
    u8g2.setFont(u8g2_font_adventurer_tr);
    int fullW = u8g2.getUTF8Width(title);
    int x0 = (SCREEN_WIDTH - fullW) / 2;
    int yBase = 32;

    char buf[16];
    for (int n = 1; n <= len; n++) {
        // Glitch frame: scrambled noise around current title.
        oled.clearDisplay();
        for (int i = 0; i < 40; i++) {
            int gx = prand(rs) % SCREEN_WIDTH;
            int gy = prand(rs) % SCREEN_HEIGHT;
            oled.drawPixel(gx, gy, SSD1306_WHITE);
        }
        memcpy(buf, title, n);
        buf[n] = '\0';
        u8g2.setCursor(x0, yBase);
        u8g2.print(buf);
        oled.display();
        frameDelay(45);

        // Clean frame
        oled.clearDisplay();
        u8g2.setCursor(x0, yBase);
        u8g2.print(buf);
        // Underline grows with reveal
        int uw = (fullW * n) / len;
        oled.drawFastHLine(x0, yBase + 4, uw, SSD1306_WHITE);
        oled.display();
        frameDelay(70);
    }
}

// Stage C: slide-in subtitle "by little hakr".
void stageSubtitle(Adafruit_SSD1306& oled, U8G2_FOR_ADAFRUIT_GFX& u8g2) {
    const char* title = "STARBEAM";
    const char* sub   = "by little hakr";
    u8g2.setFont(u8g2_font_adventurer_tr);
    int fullW = u8g2.getUTF8Width(title);
    int x0 = (SCREEN_WIDTH - fullW) / 2;
    int yBase = 32;

    u8g2.setFont(u8g2_font_baby_tf);
    int subW = u8g2.getUTF8Width(sub);
    int subTarget = (SCREEN_WIDTH - subW) / 2;
    int subY = 48;

    // 12 frames sliding from right edge to subTarget.
    for (int f = 0; f <= 12; f++) {
        oled.clearDisplay();

        // Persisted title
        u8g2.setFont(u8g2_font_adventurer_tr);
        u8g2.setCursor(x0, yBase);
        u8g2.print(title);
        oled.drawFastHLine(x0, yBase + 4, fullW, SSD1306_WHITE);

        // Sliding subtitle
        int x = SCREEN_WIDTH - ((SCREEN_WIDTH - subTarget) * f) / 12;
        u8g2.setFont(u8g2_font_baby_tf);
        u8g2.setCursor(x, subY);
        u8g2.print(sub);

        oled.display();
        frameDelay(45);
    }
}

// Stage D: scanline sweep over the held title, then decorative rules.
void stageScanlineHold(Adafruit_SSD1306& oled, U8G2_FOR_ADAFRUIT_GFX& u8g2) {
    const char* title = "STARBEAM";
    const char* sub   = "by little hakr";

    u8g2.setFont(u8g2_font_adventurer_tr);
    int fullW = u8g2.getUTF8Width(title);
    int x0 = (SCREEN_WIDTH - fullW) / 2;
    int yBase = 32;

    u8g2.setFont(u8g2_font_baby_tf);
    int subW = u8g2.getUTF8Width(sub);
    int subX = (SCREEN_WIDTH - subW) / 2;

    auto drawBase = [&]() {
        oled.clearDisplay();
        u8g2.setFont(u8g2_font_adventurer_tr);
        u8g2.setCursor(x0, yBase);
        u8g2.print(title);
        oled.drawFastHLine(x0, yBase + 4, fullW, SSD1306_WHITE);
        u8g2.setFont(u8g2_font_baby_tf);
        u8g2.setCursor(subX, 48);
        u8g2.print(sub);
    };

    // Scanline sweeps top->bottom inverting a 3px band.
    for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
        drawBase();
        oled.fillRect(0, y, SCREEN_WIDTH, 3, SSD1306_INVERSE);
        oled.display();
        frameDelay(15);
    }

    // Decorative rules grow from center outward, then hold.
    for (int w = 0; w <= SCREEN_WIDTH / 2; w += 6) {
        drawBase();
        int mid = SCREEN_WIDTH / 2;
        oled.drawFastHLine(mid - w, 12, w * 2, SSD1306_WHITE);
        oled.drawFastHLine(mid - w, 56, w * 2, SSD1306_WHITE);
        oled.display();
        frameDelay(20);
    }

    // Final hold (~400ms).
    frameDelay(400);
}

}  // namespace

void Display::playBootAnimation() {
    // Reset cache so any post-boot screens redraw cleanly.
    last_kind_info = false;
    last_menu_sel = -1;
    last_menu_first = -1;

    stageStarfield(oled, 900);
    stageTitleReveal(oled, u8g2);
    stageSubtitle(oled, u8g2);
    stageScanlineHold(oled, u8g2);
}

void Display::displayLegalWarning() {
    // Reset cache so subsequent identical info screens redraw cleanly.
    last_kind_info = false;
    last_menu_sel = -1;

    oled.clearDisplay();
    drawHeaderBar(oled, "! LEGAL WARNING");
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(2, 18);
    oled.print("Security testing");
    oled.setCursor(2, 28);
    oled.print("is for AUTHORIZED");
    oled.setCursor(2, 38);
    oled.print("use ONLY.");
    oled.drawFastHLine(0, 52, SCREEN_WIDTH, SSD1306_WHITE);
    oled.setCursor(2, 56);
    oled.print("[SELECT] I accept");
    oled.display();
}
