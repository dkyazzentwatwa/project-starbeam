#ifndef STARBEAM_WEBSERVER_H
#define STARBEAM_WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

class StarbeamWebServer {
public:
    static void init();          // Initialize (don't start yet)
    static void start();         // Start AP and web server
    static void stop();          // Stop web server and AP
    static void handleClient();  // Process requests (call in loop)
    static bool isRunning();     // Check if server active
    static String getIP();       // Get AP IP address
    static void setStaMode(bool enabled);
    static bool isStaMode();

private:
    // HTTP request handlers
    static void handleRoot();
    static void handleWiFi();
    static void handleBLE();
    static void handleNRF();
    static void handleNRFJSON();
    static void handleWiFiJSON();
    static void handleBLEJSON();
    static void handleNotFound();
    static void handleScanControl();    // POST endpoint for scan toggles
    static void handleStatus();         // GET endpoint for scan status
    static void handleSecurityRoot();   // Security testing dashboard
    static void handleStartDeauth();    // POST endpoint for starting deauth attacks
    static void handleStartBeacon();    // POST endpoint for starting beacon flood
    static void handleStartProbe();     // POST endpoint for starting probe flood
    static void handleStartPMKID();     // POST endpoint for starting PMKID capture
    static void handleStopAttack();     // POST endpoint for stopping attacks
    static void handleAttackStatus();   // GET endpoint for attack status JSON
    static void handlePMKIDList();      // GET endpoint for PMKID history JSON
    static void handlePMKIDDownload();  // GET endpoint to download PMKIDs (hashcat format)
    static void handlePMKIDClear();     // POST endpoint to clear PMKID history
    static void handleWiFiSync();
    static void handleRFScan();
    static void handleRFCapture();
    static void handleRFReplay();
    static void handleRFInfo();
    static void handleRFClear();    
    // HTML generation helpers
    static String generateHTMLHeader(const char* title);
    static String generateHTMLFooter();
    static String generateWiFiTable();
    static String generateBLETable();
    static String generateNavigation();
    static String getSecurityString(wifi_auth_mode_t authMode);

    // State
    static WebServer* server;
    static DNSServer* dnsServer;        // DNS server for network routing
    static bool running;
    static IPAddress apIP;
    static bool wifiScanEnabled;        // Background WiFi scanning state
    static bool bleScanEnabled;         // Background BLE scanning state
    static bool staModeEnabled;
};

#endif
