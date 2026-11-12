#include "webserver.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "wifi_attack.h"
#include "display.h"
#include "../config.h"

// Static member initialization
WebServer* StarbeamWebServer::server = nullptr;
DNSServer* StarbeamWebServer::dnsServer = nullptr;
bool StarbeamWebServer::running = false;
IPAddress StarbeamWebServer::apIP(192, 168, 4, 1);
bool StarbeamWebServer::wifiScanEnabled = false;
bool StarbeamWebServer::bleScanEnabled = false;
bool StarbeamWebServer::staModeEnabled = true; // Default: Try to connect to WiFi

void StarbeamWebServer::init() {
    server = nullptr;
    dnsServer = nullptr;
    running = false;
}

void StarbeamWebServer::setStaMode(bool enabled) {
    staModeEnabled = enabled;
}

bool StarbeamWebServer::isStaMode() {
    return staModeEnabled;
}

void StarbeamWebServer::start() {
    if (running) return;

    Serial.println("Starting web server...");
    bool connectedToSTA = false;

    if (staModeEnabled) {
        Serial.println("[WebServer] STA Mode enabled. Searching for configured networks...");
        WiFi.mode(WIFI_STA); // Turn off AP initially

        for (int i = 0; i < MAX_STA_NETWORKS; i++) {
            if (String(STA_SSIDS[i]).length() == 0) continue;

            Serial.printf("[WebServer] Trying to connect to: %s\n", STA_SSIDS[i]);
            WiFi.begin(STA_SSIDS[i], STA_PASSWORDS[i]);

            unsigned long startAttempt = millis();
            // Wait up to 7 seconds per network
            while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 7000) {
                delay(500);
                Serial.print(".");
                yield();
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\n[WebServer] Connected to STA!");
                connectedToSTA = true;
                break;
            } else {
                Serial.println("\n[WebServer] Failed to connect.");
                WiFi.disconnect();
            }
        }
    }

    // Failsafe: If STA failed or is disabled, turn on the AP
    if (!connectedToSTA) {
        Serial.println("[WebServer] Falling back to Access Point mode.");
        // CRITICAL: Use AP_STA mode to allow WiFi scanning while AP is active
        WiFi.mode(WIFI_AP_STA);
        
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        delay(500);  // Let AP stabilize

        // Start DNS server (captive portal)
        dnsServer = new DNSServer();
        dnsServer->start(53, "*", apIP);
    } else {
        // We are connected to a router. We use AP_STA anyway to allow scanning.
        // But we don't start the AP or DNS server to save power and avoid interference.
        WiFi.mode(WIFI_AP_STA); 
        Serial.print("[WebServer] STA IP: ");
        Serial.println(WiFi.localIP());
    }

    // Create and configure HTTP server
    server = new WebServer(WEB_SERVER_PORT);

    // Main endpoints
    server->on("/", handleRoot);
    server->on("/wifi", handleWiFi);
    server->on("/ble", handleBLE);
    server->on("/api/wifi", handleWiFiJSON);
    server->on("/api/ble", handleBLEJSON);
    server->on("/api/scan/control", HTTP_POST, handleScanControl);
    server->on("/api/scan/status", HTTP_GET, handleStatus);

    // Security testing endpoints
    server->on("/security", handleSecurityRoot);
    server->on("/api/attack/deauth", HTTP_POST, handleStartDeauth);
    server->on("/api/attack/beacon", HTTP_POST, handleStartBeacon);
    server->on("/api/attack/probe", HTTP_POST, handleStartProbe);
    server->on("/api/attack/pmkid", HTTP_POST, handleStartPMKID);
    server->on("/api/attack/stop", HTTP_POST, handleStopAttack);
    server->on("/api/attack/status", HTTP_GET, handleAttackStatus);
    server->on("/api/pmkid/list", HTTP_GET, handlePMKIDList);
    server->on("/api/pmkid/download", HTTP_GET, handlePMKIDDownload);
    server->on("/api/pmkid/clear", HTTP_POST, handlePMKIDClear);

    server->onNotFound(handleNotFound);

    server->begin();
    running = true;

    String currentIP = connectedToSTA ? WiFi.localIP().toString() : apIP.toString();
    Serial.printf("Web server started at http://%s\n", currentIP.c_str());

    // Show IP on OLED
    Display::displayInfo("WEB SERVER",
                         "Mode: " + String(connectedToSTA ? "WiFi STA" : "Access Point"),
                         "IP: " + currentIP,
                         "Captive Portal: " + String(connectedToSTA ? "OFF" : "ON"));
    delay(2000);
}

void StarbeamWebServer::stop() {
    if (!running) return;

    Serial.println("Stopping web server...");

    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }

    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }

    // Disconnect everything
    WiFi.disconnect(true); 
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    running = false;
}

void StarbeamWebServer::handleClient() {
    if (running) {
        if (dnsServer) {
            dnsServer->processNextRequest();  // Process DNS queries
        }
        if (server) {
            server->handleClient();  // Handle HTTP requests
        }
    }
}

bool StarbeamWebServer::isRunning() {
    return running;
}

String StarbeamWebServer::getIP() {
    // Return STA IP if connected, otherwise AP IP
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return apIP.toString();
}

// HTML Generation Helpers

String StarbeamWebServer::generateHTMLHeader(const char* title) {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>" + String(title) + " - Starbeam</title>";
    html += "<style>";
    html += "body{font-family:monospace;margin:20px;background:#1a1a1a;color:#00ff00}";
    html += "h1,h2{color:#00ff00}";
    html += "table{border-collapse:collapse;width:100%}";
    html += "th{background:#333;padding:8px;text-align:left}";
    html += "td{padding:6px;border-bottom:1px solid #444}";
    html += "tr:hover{background:#222}";
    html += "a,button{color:#00aaff;text-decoration:none;margin:0 10px}";
    html += "a:hover{text-decoration:underline}";
    html += ".nav{margin:20px 0;padding:10px;background:#333}";
    html += "button{background:#333;border:1px solid #555;padding:5px 10px;cursor:pointer}";
    html += "button:hover{background:#444}";
    html += "</style></head><body>";
    html += "<h1>&#x1F4E1; Project Starbeam V2</h1>";
    return html;
}

String StarbeamWebServer::generateNavigation() {
    String nav = "<div class='nav'>";
    nav += "<a href='/'>Home</a>";
    nav += "<a href='/wifi'>WiFi Scanner</a>";
    nav += "<a href='/ble'>BLE Scanner</a>";
    nav += "<a href='/security' style='color:#ff6600;'>&#128274; Security</a>";
    nav += "<a href='/api/wifi'>WiFi JSON</a>";
    nav += "<a href='/api/ble'>BLE JSON</a>";
    nav += "</div>";
    return nav;
}

String StarbeamWebServer::generateHTMLFooter() {
    String html = "<hr><p style='color:#666;font-size:10px;'>";
    html += "Starbeam V2 &copy; 2024 | Auto-refresh: 5s</p>";
    html += "</body></html>";
    return html;
}

String StarbeamWebServer::getSecurityString(wifi_auth_mode_t authMode) {
    switch (authMode) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Ent";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "Unknown";
    }
}

String StarbeamWebServer::generateWiFiTable() {
    String table;
    table.reserve(2500);  // Pre-allocate to reduce fragmentation

    table = "<table border='1' cellpadding='5'>";
    table += "<tr><th>#</th><th>SSID</th><th>BSSID</th><th>RSSI</th><th>Ch</th><th>Security</th></tr>";

    int count = WiFiScanner::getNetworkCount();

    if (count == 0) {
        table += "<tr><td colspan='6' style='text-align:center;color:#ff6600;'>";
        table += "No networks found. Scanner may not be running.</td></tr>";
    } else {
        for (int i = 0; i < count; i++) {
            const WiFiNetworkInfo* net = WiFiScanner::getNetwork(i);
            if (net) {
                table += "<tr>";
                table += "<td>" + String(i + 1) + "</td>";
                table += "<td>" + (net->ssid.length() > 0 ? net->ssid : "[Hidden]") + "</td>";
                table += "<td style='font-size:10px;'>" + net->bssid + "</td>";

                // Color-code RSSI (green=strong, yellow=medium, red=weak)
                String rssiColor = "#00ff00";  // Green
                if (net->rssi < -70) rssiColor = "#ff6600";  // Orange
                if (net->rssi < -80) rssiColor = "#ff0000";  // Red
                table += "<td style='color:" + rssiColor + ";'>" + String(net->rssi) + " dBm</td>";

                table += "<td>" + String(net->channel) + "</td>";
                table += "<td>" + getSecurityString(net->authMode) + "</td>";
                table += "</tr>";

                // Yield every 5 rows to prevent watchdog timeout
                if (i % 5 == 4) yield();
            }
        }
    }

    table += "</table>";
    return table;
}

String StarbeamWebServer::generateBLETable() {
    String table;
    table.reserve(3000);  // Pre-allocate

    table = "<table border='1' cellpadding='5'>";
    table += "<tr><th>#</th><th>Name</th><th>MAC Address</th><th>RSSI</th><th>Type</th><th>Connectable</th></tr>";

    int count = BLEScanner::getDeviceCount();

    if (count == 0) {
        table += "<tr><td colspan='6' style='text-align:center;color:#ff6600;'>";
        table += "No devices found. Scanner may not be running.</td></tr>";
    } else {
        for (int i = 0; i < count; i++) {
            const BLEDeviceInfo* dev = BLEScanner::getDevice(i);
            if (dev) {
                table += "<tr>";
                table += "<td>" + String(i + 1) + "</td>";
                table += "<td>" + String(dev->name) + "</td>";
                table += "<td style='font-size:10px;'>" + String(dev->address) + "</td>";

                // Color-code RSSI
                String rssiColor = "#00ff00";
                if (dev->rssi < -70) rssiColor = "#ff6600";
                if (dev->rssi < -80) rssiColor = "#ff0000";
                table += "<td style='color:" + rssiColor + ";'>" + String(dev->rssi) + " dBm</td>";

                table += "<td>" + String(dev->addressType == 0 ? "Public" : "Random") + "</td>";
                table += "<td>" + String(dev->connectable ? "Yes" : "No") + "</td>";
                table += "</tr>";

                // Yield every 5 rows
                if (i % 5 == 4) yield();
            }
        }
    }

    table += "</table>";
    return table;
}

// HTTP Request Handlers

void StarbeamWebServer::handleRoot() {
    String html = generateHTMLHeader("Dashboard");
    html += generateNavigation();

    // Scan Control Section
    html += "<h2>Scan Controls</h2>";
    html += "<div style='margin:20px 0;'>";
    html += "<button onclick='toggleScan(\"wifi\")' style='padding:10px 20px;margin:5px;background:#333;color:#00ff00;border:1px solid #00ff00;cursor:pointer;'>Toggle WiFi Scan</button>";
    html += "<button onclick='toggleScan(\"ble\")' style='padding:10px 20px;margin:5px;background:#333;color:#00ff00;border:1px solid #00ff00;cursor:pointer;'>Toggle BLE Scan</button>";
    html += "</div>";

    // Status table
    html += "<h2>Scanner Status</h2>";
    html += "<table border='1' cellpadding='10' style='width:50%;'>";
    html += "<tr><th>Scanner</th><th>Status</th><th>Results</th></tr>";

    int wifiCount = WiFiScanner::getNetworkCount();
    String wifiStatus = wifiScanEnabled ? "Scanning" : "Stopped";
    html += "<tr><td>WiFi</td><td style='color:" + String(wifiScanEnabled ? "#00ff00" : "#ff6600") + ";'>" + wifiStatus + "</td>";
    html += "<td>" + String(wifiCount) + " networks</td></tr>";

    int bleCount = BLEScanner::getDeviceCount();
    String bleStatus = bleScanEnabled ? "Scanning" : "Stopped";
    html += "<tr><td>BLE</td><td style='color:" + String(bleScanEnabled ? "#00ff00" : "#ff6600") + ";'>" + bleStatus + "</td>";
    html += "<td>" + String(bleCount) + " devices</td></tr>";

    html += "</table>";
    html += "<p style='margin-top:30px;'>Select a scanner above to view detailed results.</p>";

    // JavaScript for toggle buttons
    html += "<script>";
    html += "function toggleScan(type) {";
    html += "  var newVal = (type=='wifi') ? (" + String(wifiScanEnabled ? "0" : "1") + ") : (" + String(bleScanEnabled ? "0" : "1") + ");";
    html += "  fetch('/api/scan/control?'+type+'='+newVal).then(()=>location.reload());";
    html += "}";
    html += "</script>";

    html += generateHTMLFooter();

    server->send(200, "text/html", html);
}

void StarbeamWebServer::handleWiFi() {
    unsigned long startTime = millis();

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", "");

    server->sendContent(generateHTMLHeader("WiFi Scanner"));
    yield();
    server->sendContent(generateNavigation());
    yield();

    String header = "<button onclick='location.reload()'>&#128472; Refresh</button>";
    header += "<h2>WiFi Networks (" + String(WiFiScanner::getNetworkCount()) + ")</h2>";
    server->sendContent(header);
    yield();

    int count = WiFiScanner::getNetworkCount();
    String chunk = "<table border='1' cellpadding='5'>";
    chunk += "<tr><th>#</th><th>SSID</th><th>BSSID</th><th>RSSI</th><th>Ch</th><th>Security</th></tr>";
    server->sendContent(chunk);
    yield();

    if (count == 0) {
        server->sendContent("<tr><td colspan='6' style='text-align:center;color:#ff6600;'>No networks found.</td></tr>");
    } else {
        for (int i = 0; i < count; i++) {
            const WiFiNetworkInfo* net = WiFiScanner::getNetwork(i);
            if (net) {
                String row = "<tr>";
                row += "<td>" + String(i) + "</td>";
                row += "<td>" + (net->ssid.length() > 0 ? net->ssid : "[Hidden]") + "</td>";
                row += "<td style='font-size:10px;'>" + net->bssid + "</td>";

                String rssiColor = net->rssi > -70 ? "#00ff00" : (net->rssi > -80 ? "#ff6600" : "#ff0000");
                row += "<td style='color:" + rssiColor + ";'>" + String(net->rssi) + " dBm</td>";
                row += "<td>" + String(net->channel) + "</td>";
                row += "<td>" + getSecurityString(net->authMode) + "</td>";
                row += "</tr>";

                server->sendContent(row);
                if (i % 3 == 0) yield();
            }
        }
    }

    server->sendContent("</table>");
    yield();
    server->sendContent(generateHTMLFooter());
    server->sendContent("");

    Serial.printf("[WebServer] WiFi page sent in %lu ms\n", millis() - startTime);
}

void StarbeamWebServer::handleBLE() {
    unsigned long startTime = millis();

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", "");

    server->sendContent(generateHTMLHeader("BLE Scanner"));
    yield();
    server->sendContent(generateNavigation());
    yield();

    String header = "<button onclick='location.reload()'>&#128472; Refresh</button>";
    header += "<h2>BLE Devices (" + String(BLEScanner::getDeviceCount()) + ")</h2>";
    server->sendContent(header);
    yield();

    int count = BLEScanner::getDeviceCount();
    String chunk = "<table border='1' cellpadding='5'>";
    chunk += "<tr><th>#</th><th>Name</th><th>MAC Address</th><th>RSSI</th><th>Type</th><th>Connectable</th></tr>";
    server->sendContent(chunk);
    yield();

    if (count == 0) {
        server->sendContent("<tr><td colspan='6' style='text-align:center;color:#ff6600;'>No devices found.</td></tr>");
    } else {
        for (int i = 0; i < count; i++) {
            const BLEDeviceInfo* dev = BLEScanner::getDevice(i);
            if (dev) {
                String row = "<tr>";
                row += "<td>" + String(i) + "</td>";
                row += "<td>" + String(strlen(dev->name) > 0 ? dev->name : "[Unknown]") + "</td>";
                row += "<td style='font-size:10px;'>" + String(dev->address) + "</td>";

                String rssiColor = dev->rssi > -70 ? "#00ff00" : (dev->rssi > -80 ? "#ff6600" : "#ff0000");
                row += "<td style='color:" + rssiColor + ";'>" + String(dev->rssi) + " dBm</td>";
                row += "<td>" + String(dev->addressType == 0 ? "Public" : "Random") + "</td>";
                row += "<td>" + String(dev->connectable ? "Yes" : "No") + "</td>";
                row += "</tr>";

                server->sendContent(row);
                if (i % 3 == 0) yield();
            }
        }
    }

    server->sendContent("</table>");
    yield();
    server->sendContent(generateHTMLFooter());
    server->sendContent("");

    Serial.printf("[WebServer] BLE page sent in %lu ms\n", millis() - startTime);
}

void StarbeamWebServer::handleWiFiJSON() {
    String json = "{\"networks\":[";

    int count = WiFiScanner::getNetworkCount();
    for (int i = 0; i < count; i++) {
        const WiFiNetworkInfo* net = WiFiScanner::getNetwork(i);
        if (net) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"ssid\":\"" + net->ssid + "\",";
            json += "\"bssid\":\"" + net->bssid + "\",";
            json += "\"rssi\":" + String(net->rssi) + ",";
            json += "\"channel\":" + String(net->channel) + ",";
            json += "\"security\":\"" + getSecurityString(net->authMode) + "\"";
            json += "}";
        }
    }

    json += "],\"count\":" + String(count) + "}";

    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->send(200, "application/json", json);
}

void StarbeamWebServer::handleBLEJSON() {
    String json = "{\"devices\":[";

    int count = BLEScanner::getDeviceCount();
    for (int i = 0; i < count; i++) {
        const BLEDeviceInfo* dev = BLEScanner::getDevice(i);
        if (dev) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"name\":\"" + String(dev->name) + "\",";
            json += "\"address\":\"" + String(dev->address) + "\",";
            json += "\"rssi\":" + String(dev->rssi) + ",";
            json += "\"addressType\":\"" + String(dev->addressType == 0 ? "Public" : "Random") + "\",";
            json += "\"connectable\":" + String(dev->connectable ? "true" : "false");
            json += "}";
        }
    }

    json += "],\"count\":" + String(count) + "}";

    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->send(200, "application/json", json);
}

void StarbeamWebServer::handleNotFound() {
    server->sendHeader("Location", "/", true);
    server->send(302, "text/plain", "");
}

void StarbeamWebServer::handleScanControl() {
    if (server->hasArg("wifi")) {
        wifiScanEnabled = server->arg("wifi") == "1";
    }
    if (server->hasArg("ble")) {
        bleScanEnabled = server->arg("ble") == "1";
    }

    String json = "{";
    json += "\"wifi\":" + String(wifiScanEnabled ? "true" : "false") + ",";
    json += "\"ble\":" + String(bleScanEnabled ? "true" : "false");
    json += "}";

    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->send(200, "application/json", json);
}

void StarbeamWebServer::handleStatus() {
    String json = "{";
    json += "\"wifi_scan\":" + String(wifiScanEnabled ? "true" : "false") + ",";
    json += "\"ble_scan\":" + String(bleScanEnabled ? "true" : "false") + ",";
    json += "\"wifi_count\":" + String(WiFiScanner::getNetworkCount()) + ",";
    json += "\"ble_count\":" + String(BLEScanner::getDeviceCount());
    json += "}";

    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->send(200, "application/json", json);
}

// Security Testing Handlers

void StarbeamWebServer::handleSecurityRoot() {
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", "");

    server->sendContent(generateHTMLHeader("Security Testing"));
    yield();
    server->sendContent(generateNavigation());
    yield();

    server->sendContent("<button onclick='location.reload()'>&#128472; Refresh</button>");
    yield();

    String warning = "<div style='background:#ff0000;color:#ffffff;padding:20px;margin:20px 0;border:2px solid #ffffff;'>";
    warning += "<h2 style='color:#ffffff;margin:0;'>&#9888;&#65039; LEGAL WARNING &#9888;&#65039;</h2>";
    warning += "<p>These tools are for AUTHORIZED security testing ONLY.</p>";
    warning += "<p>Unauthorized use may violate laws in your country.</p>";
    warning += "<p>You are responsible for compliance with all applicable laws.</p></div>";
    server->sendContent(warning);
    yield();

    if (WiFiAttack::isAttacking()) {
        String status = "<div style='background:#ff6600;padding:15px;margin:10px 0;'>";
        status += "<h3>&#128680; ATTACK IN PROGRESS</h3>";

        String attackType;
        switch (WiFiAttack::getAttackType()) {
            case 0: attackType = "Targeted Deauth"; break;
            case 1: attackType = "Broadcast Deauth"; break;
            case 2: attackType = "Beacon Flood"; break;
            case 3: attackType = "Probe Flood"; break;
            case 4: attackType = "PMKID Capture"; break;
            default: attackType = "Unknown"; break;
        }

        status += "<p><strong>Type:</strong> " + attackType + "</p>";
        status += "<p><strong>Frames Sent:</strong> " + String(WiFiAttack::getFramesSent()) + "</p>";
        status += "<p><strong>Stations Eliminated:</strong> " + String(WiFiAttack::getStationsEliminated()) + "</p>";
        status += "<form method='post' action='/api/attack/stop' style='display:inline;'>";
        status += "<button type='submit' style='background:#ff0000;color:#fff;padding:10px 20px;border:none;cursor:pointer;'>STOP ATTACK</button>";
        status += "</form></div>";
        server->sendContent(status);
        yield();
    }

    server->sendContent("<h2>Available Networks</h2>");
    yield();

    int netCount = WiFiScanner::getNetworkCount();
    String tableStart = "<table border='1' cellpadding='5'>";
    tableStart += "<tr><th>#</th><th>SSID</th><th>BSSID</th><th>RSSI</th><th>Ch</th><th>Security</th></tr>";
    server->sendContent(tableStart);

    for (int i = 0; i < netCount; i++) {
        const WiFiNetworkInfo* net = WiFiScanner::getNetwork(i);
        if (net) {
            String row = "<tr><td>" + String(i) + "</td>";
            row += "<td>" + (net->ssid.length() > 0 ? net->ssid : "[Hidden]") + "</td>";
            row += "<td style='font-size:10px;'>" + net->bssid + "</td>";
            String rssiColor = net->rssi > -70 ? "#00ff00" : (net->rssi > -80 ? "#ff6600" : "#ff0000");
            row += "<td style='color:" + rssiColor + ";'>" + String(net->rssi) + " dBm</td>";
            row += "<td>" + String(net->channel) + "</td>";
            row += "<td>" + getSecurityString(net->authMode) + "</td></tr>";
            server->sendContent(row);
            if (i % 2 == 0) yield();
        }
    }
    server->sendContent("</table>");
    yield();

    String controls = "<h2>Attack Controls</h2>";
    controls += "<h3>Targeted Deauthentication</h3>";
    controls += "<form method='post' action='/api/attack/deauth'>";
    controls += "Network Index: <input type='number' name='network' min='0' placeholder='0' required> ";
    controls += "Reason: <select name='reason'>";
    controls += "<option value='1'>Unspecified</option>";
    controls += "<option value='3'>Leaving BSS</option>";
    controls += "<option value='7' selected>Class 3 frame</option></select> ";
    controls += "<button type='submit' style='background:#ff6600;color:#fff;padding:8px 15px;border:none;cursor:pointer;'>Start</button>";
    controls += "</form>";
    server->sendContent(controls);
    yield();

    String broadcast = "<h3>Broadcast Deauthentication</h3>";
    broadcast += "<p style='color:#ff6600;font-size:12px;'>&#9888; WARNING: Disrupts ALL nearby networks.</p>";
    broadcast += "<form method='post' action='/api/attack/deauth'>";
    broadcast += "<input type='hidden' name='network' value='-1'>";
    broadcast += "<input type='hidden' name='reason' value='7'>";
    broadcast += "<button type='submit' style='background:#ff0000;color:#fff;padding:8px 15px;border:none;cursor:pointer;'>Start Broadcast</button>";
    broadcast += "</form>";
    server->sendContent(broadcast);
    yield();

    String beacon = "<h3>Beacon Flooding</h3>";
    beacon += "<p style='color:#999;font-size:12px;'>Creates 20 fake access points.</p>";
    beacon += "<form method='post' action='/api/attack/beacon'>";
    beacon += "<button type='submit' style='background:#ff6600;color:#fff;padding:8px 15px;border:none;cursor:pointer;'>Start</button></form>";
    server->sendContent(beacon);
    yield();

    String probe = "<h3>Probe Request Flooding</h3>";
    probe += "<p style='color:#999;font-size:12px;'>Simulates multiple clients searching.</p>";
    probe += "<form method='post' action='/api/attack/probe'>";
    probe += "<button type='submit' style='background:#ff6600;color:#fff;padding:8px 15px;border:none;cursor:pointer;'>Start</button></form>";
    server->sendContent(probe);
    yield();

    String pmkid = "<h3>PMKID Capture</h3>";
    pmkid += "<p style='color:#999;font-size:12px;'>Captures WPA2/WPA3 handshake data.</p>";
    pmkid += "<form method='post' action='/api/attack/pmkid'>";
    pmkid += "Network Index: <input type='number' name='network' min='0' placeholder='0' required> ";
    pmkid += "<button type='submit' style='background:#ff6600;color:#fff;padding:8px 15px;border:none;cursor:pointer;'>Start</button></form>";
    server->sendContent(pmkid);
    yield();

    server->sendContent("<h2>Captured PMKIDs</h2>");
    int pmkidCount = WiFiAttack::getPMKIDHistoryCount();

    if (pmkidCount > 0) {
        String buttons = "<p>Total: <strong>" + String(pmkidCount) + "</strong></p>";
        buttons += "<form method='get' action='/api/pmkid/download' style='display:inline;margin-right:10px;'>";
        buttons += "<button type='submit' style='background:#0066cc;color:#fff;padding:8px 15px;border:none;cursor:pointer;'>Download</button></form>";
        buttons += "<form method='post' action='/api/pmkid/clear' style='display:inline;'>";
        buttons += "<button type='submit' style='background:#666;color:#fff;padding:8px 15px;border:none;cursor:pointer;'>Clear</button></form>";
        server->sendContent(buttons);
        yield();

        String tableHdr = "<table style='width:100%;margin-top:15px;border-collapse:collapse;'>";
        tableHdr += "<tr style='background:#333;'>";
        tableHdr += "<th style='padding:8px;text-align:left;border:1px solid #555;'>SSID</th>";
        tableHdr += "<th style='padding:8px;text-align:left;border:1px solid #555;'>AP MAC</th>";
        tableHdr += "<th style='padding:8px;text-align:left;border:1px solid #555;'>PMKID</th></tr>";
        server->sendContent(tableHdr);

        for (int i = 0; i < pmkidCount; i++) {
            const pmkid_capture_t* capture = WiFiAttack::getPMKIDHistory(i);
            if (capture && capture->valid) {
                String row = "<tr style='background:#222;'>";
                row += "<td style='padding:8px;border:1px solid #555;'>" + String(capture->ssid) + "</td>";

                row += "<td style='padding:8px;border:1px solid #555;font-family:monospace;font-size:11px;'>";
                for (int j = 0; j < 6; j++) {
                    char hex[3];
                    sprintf(hex, "%02x", capture->ap_mac[j]);
                    row += hex;
                    if (j < 5) row += ":";
                }
                row += "</td>";

                row += "<td style='padding:8px;border:1px solid #555;font-family:monospace;font-size:11px;'>";
                for (int j = 0; j < 8; j++) {
                    char hex[3];
                    sprintf(hex, "%02x", capture->pmkid[j]);
                    row += hex;
                }
                row += "...</td></tr>";

                server->sendContent(row);
                yield();
            }
        }
        server->sendContent("</table>");
    } else {
        server->sendContent("<p style='color:#999;'>No PMKID captures yet.</p>");
    }

    yield();
    server->sendContent(generateHTMLFooter());
    server->sendContent("");
}

void StarbeamWebServer::handleStartDeauth() {
    if (!server->hasArg("network") || !server->hasArg("reason")) {
        server->send(400, "text/plain", "Missing parameters");
        return;
    }

    int network = server->arg("network").toInt();
    int reason = server->arg("reason").toInt();

    WiFiAttack::init();

    if (network == -1) {
        WiFiAttack::startDeauthBroadcast(reason);
        Serial.println("[WebServer] Started broadcast deauth attack");
    } else {
        WiFiAttack::startDeauthTargeted(network, reason);
        Serial.printf("[WebServer] Started targeted deauth attack on network %d\n", network);
    }

    server->sendHeader("Location", "/security");
    server->send(302);
}

void StarbeamWebServer::handleStopAttack() {
    WiFiAttack::stopAttack();
    WiFiAttack::deinit();
    Serial.println("[WebServer] Stopped attack");
    server->sendHeader("Location", "/security");
    server->send(302);
}

void StarbeamWebServer::handleAttackStatus() {
    String json = "{";
    json += "\"attacking\":" + String(WiFiAttack::isAttacking() ? "true" : "false") + ",";
    json += "\"type\":" + String(WiFiAttack::getAttackType()) + ",";
    json += "\"frames_sent\":" + String(WiFiAttack::getFramesSent()) + ",";
    json += "\"stations_eliminated\":" + String(WiFiAttack::getStationsEliminated());
    json += "}";

    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->send(200, "application/json", json);
}

void StarbeamWebServer::handleStartBeacon() {
    WiFiAttack::init();
    const char* fake_ssids[20] = {
        "Free WiFi", "Airport WiFi", "Guest Network", "Starbucks WiFi",
        "xfinitywifi", "ATT WiFi", "Verizon WiFi", "Public WiFi",
        "Hotel Guest", "Conference WiFi", "Open Network", "Guest",
        "Visitor WiFi", "Free Internet", "WiFi Access", "Network",
        "Internet", "WiFi", "Wireless", "Public"
    };
    WiFiAttack::startBeaconFlood(fake_ssids, 20);
    Serial.println("[WebServer] Started beacon flooding attack");
    server->sendHeader("Location", "/security");
    server->send(302);
}

void StarbeamWebServer::handleStartProbe() {
    WiFiAttack::init();
    WiFiAttack::startProbeFlood("");
    Serial.println("[WebServer] Started probe flooding attack");
    server->sendHeader("Location", "/security");
    server->send(302);
}

void StarbeamWebServer::handleStartPMKID() {
    if (!server->hasArg("network")) {
        server->send(400, "text/plain", "Missing network parameter");
        return;
    }
    int network = server->arg("network").toInt();
    WiFiAttack::init();
    WiFiAttack::startPMKIDCapture(network);
    Serial.printf("[WebServer] Started PMKID capture on network %d\n", network);
    server->sendHeader("Location", "/security");
    server->send(302);
}

void StarbeamWebServer::handlePMKIDList() {
    String json = "{";
    json += "\"count\":" + String(WiFiAttack::getPMKIDHistoryCount()) + ",";
    json += "\"captures\":[";

    for (int i = 0; i < WiFiAttack::getPMKIDHistoryCount(); i++) {
        const pmkid_capture_t* capture = WiFiAttack::getPMKIDHistory(i);
        if (capture && capture->valid) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"ssid\":\"" + String(capture->ssid) + "\",";
            json += "\"timestamp\":" + String(capture->timestamp) + ",";
            json += "\"ap_mac\":\"";
            for (int j = 0; j < 6; j++) {
                char hex[3]; sprintf(hex, "%02x", capture->ap_mac[j]); json += hex;
                if (j < 5) json += ":";
            }
            json += "\",";
            json += "\"station_mac\":\"";
            for (int j = 0; j < 6; j++) {
                char hex[3]; sprintf(hex, "%02x", capture->station_mac[j]); json += hex;
                if (j < 5) json += ":";
            }
            json += "\",";
            json += "\"pmkid\":\"";
            for (int j = 0; j < 16; j++) {
                char hex[3]; sprintf(hex, "%02x", capture->pmkid[j]); json += hex;
            }
            json += "\"";
            json += "}";
        }
    }
    json += "]}";
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->send(200, "application/json", json);
}

void StarbeamWebServer::handlePMKIDDownload() {
    String hashcat = "";
    for (int i = 0; i < WiFiAttack::getPMKIDHistoryCount(); i++) {
        const pmkid_capture_t* capture = WiFiAttack::getPMKIDHistory(i);
        if (capture && capture->valid) {
            for (int j = 0; j < 16; j++) {
                char hex[3]; sprintf(hex, "%02x", capture->pmkid[j]); hashcat += hex;
            }
            hashcat += "*";
            for (int j = 0; j < 6; j++) {
                char hex[3]; sprintf(hex, "%02x", capture->ap_mac[j]); hashcat += hex;
            }
            hashcat += "*";
            for (int j = 0; j < 6; j++) {
                char hex[3]; sprintf(hex, "%02x", capture->station_mac[j]); hashcat += hex;
            }
            hashcat += "*";
            hashcat += String(capture->ssid);
            hashcat += "\n";
        }
    }
    if (hashcat.length() == 0) hashcat = "# No PMKID captures found\n";

    server->sendHeader("Content-Disposition", "attachment; filename=pmkids.txt");
    server->send(200, "text/plain", hashcat);
}

void StarbeamWebServer::handlePMKIDClear() {
    WiFiAttack::clearPMKIDHistory();
    Serial.println("[WebServer] Cleared PMKID history");
    server->sendHeader("Location", "/security");
    server->send(302);
}