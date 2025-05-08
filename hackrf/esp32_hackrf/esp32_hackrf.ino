#include <HardwareSerial.h>

HardwareSerial mySerial(1);  // UART1 for Raspberry Pi

void setup() {
    Serial.begin(115200);
    mySerial.begin(115200, SERIAL_8N1, 16, 17);  // TX=16, RX=17
    delay(3000);  // Allow time for Pi to boot

    Serial.println("ESP32 HackRF Controller");
    Serial.println("Enter commands (e.g., TX 915000000 10, STOP, STATUS):");
}

void loop() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');  // Read user input
        command.trim();  // Remove whitespace/newline

        if (command.length() > 0) {
            mySerial.println(command);  // Send to Raspberry Pi
            Serial.println(command);
            waitForResponse(5000);  // Wait for a response from Pi
        }
    }

    if (mySerial.available()) {  
        String response = mySerial.readStringUntil('\n');  
        Serial.println("HackRF Response: " + response);  // Display response
    }
}

void waitForResponse(int timeout) {
    long waitUntil = millis() + timeout;
    while (millis() < waitUntil) {
        if (mySerial.available()) {
            String response = mySerial.readStringUntil('\n');
            Serial.println("HackRF Response: " + response);
            return;  // Exit once a response is received
        }
    }
    Serial.println("Timeout waiting for response.");
}