  #include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HardwareSerial.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // No reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Button pins
#define BTN_UP 39
#define BTN_DOWN 34
#define BTN_SELECT 36

HardwareSerial mySerial(1);  // UART1 for Raspberry Pi

const char* commands[] = {"TX 433900000 45", "TX 915000000 45", "TX 315000000 40", "STOP", "STATUS"};
const int numCommands = sizeof(commands) / sizeof(commands[0]);
int currentSelection = 0;

void setup() {
    Serial.begin(115200);
    mySerial.begin(115200, SERIAL_8N1, 16, 17);  // TX=16, RX=17
    delay(3000);  // Allow time for Pi to boot

    // Initialize OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed");
        for (;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("ESP32 HackRF Controller");
    display.display();
    
    // Configure buttons as inputs
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
}

void loop() {
    handleButtons();
    if (mySerial.available()) {  
        String response = mySerial.readStringUntil('\n');  
        Serial.println("HackRF Response: " + response);  
        displayResponse(response);
    }
}

void handleButtons() {
    if (digitalRead(BTN_UP) == LOW) {
        currentSelection = (currentSelection - 1 + numCommands) % numCommands;
        updateDisplay();
        delay(200);
    }
    if (digitalRead(BTN_DOWN) == LOW) {
        currentSelection = (currentSelection + 1) % numCommands;
        updateDisplay();
        delay(200);
    }
    if (digitalRead(BTN_SELECT) == LOW) {
        sendCommand(commands[currentSelection]);
        delay(200);
    }
}

void updateDisplay() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Select Command:");
    display.println(commands[currentSelection]);
    display.display();
}

void sendCommand(const char* command) {
    mySerial.println(command);
    Serial.println(String(command));
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sent:");
    display.println(command);
    display.display();
    waitForResponse(5000);
}

void waitForResponse(int timeout) {
    long waitUntil = millis() + timeout;
    while (millis() < waitUntil) {
        if (mySerial.available()) {
            String response = mySerial.readStringUntil('\n');
            Serial.println("HackRF Response: " + response);
            displayResponse(response);
            return;
        }
    }
    Serial.println("Timeout waiting for response.");
    displayResponse("Timeout");
}

void displayResponse(String response) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("HackRF Response:");
    display.println(response);
    display.display();
}
