/*Full working version*/

// Device/Company Info
const String device_id = "D10001";
const String default_token = "SECRETKEY789";
const String cid = "TFL";

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "RTClib.h"
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include <SD.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

struct WiFiConfig;  // Forward declaration
const String TOKEN_FILE = "/auth_token.txt";  // File to store token
const String PENDING_ATTENDANCE_FILE = "/pending_attendance.csv"; // File paths

// ===== Attendance System Constants =====
const unsigned long SYNC_INTERVAL = 1 * 60 * 1000;  // Sync every 5 minutes
const unsigned long SYNC_RETRY_DELAY = 1 * 60 * 1000;   // Retry failed syncs every 30 seconds
const int MAX_RETRIES = 3;                          // Max retry attempts per record

// Save token to SD card
bool saveAuthToken(const String &token) {
  return saveToSD(TOKEN_FILE, token);
}

// Load token from SD card
String loadAuthToken() {
  return readFromSD(TOKEN_FILE);
}

// Delete token file (when unauthorized)
bool clearAuthToken() {
  return deleteFromSD(TOKEN_FILE);
}

// const String base_url = "http://103.209.42.168";  // Change to your server IP
const String base_url = "http://34.142.226.62:9001";  // Change to your server IP

String auth_token = "";

const size_t JSON_BUFFER_SIZE = 2048;

void enrollFingerprint(int id = -1);
bool captureFingerprint(int step, const char *prompt, const char *successMsg);
void deleteFingerprint(int id);
void listFingerprints();
void showLastLogs();

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// ===== Constants =====
const char *menuItems[] = {
  "Register Finger",
  "Delete Finger",
  "List Fingerprints",
  "Show Logs",
  "Set WiFi",
  "Exit"
};

const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);
int currentMenu = 0;

bool menuMode = false;
unsigned long menuStartTime = 0;
unsigned long lastMenuInteraction = 0;
const unsigned long menuTimeout = 30000;  // 30 seconds

#define LONG_PRESS_THRESHOLD 1000  // 1 second
volatile bool buttonPressedFlag = false;
unsigned long pressStartTime = 0;
bool buttonBeingHandled = false;

// Pin Definitions
#define I2C_SDA 23
#define I2C_SCL 22
#define SD_MISO 19
#define SD_MOSI 18
#define SD_SCK 17
#define SD_CS 16
#define FINGERPRINT_RX 27
#define FINGERPRINT_TX 26
#define TOUCH_PIN 25
#define BUTTON_PIN 21
#define BUZZER_PIN 32

// WiFi Configuration
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 6 * 3600;
const int daylightOffset_sec = 0;

// WiFi maintenance
unsigned long wifiPreviousMillis = 0;
const long wifiInterval = 30000;  // WiFi reconnect interval (30 seconds)
bool wifiConnected = false;

// WiFi icon
const uint8_t wifi_connected_icon[] PROGMEM = {
  0x00, 0x00, 0x07, 0xE0, 0x1F, 0xF8, 0x38, 0x1C,
  0x63, 0xC6, 0x47, 0xE2, 0x0C, 0x30, 0x08, 0x10,
  0x01, 0x80, 0x03, 0xC0, 0x03, 0xC0, 0x01, 0x80,
  0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Global Objects
Adafruit_SH1106G display(128, 64, &Wire, -1);
RTC_DS3231 rtc;
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);

// Global Variables
volatile bool fingerTouched = false;
int scrollPosition = 0;
unsigned long lastScrollTime = 0;
const char *websiteText = "www.eonsystem.com";
const int scrollDelay = 150;

// ===== SD Card Functions =====
bool saveToSD(const String &filename, const String &data) {
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing: " + filename);
    return false;
  }

  if (file.print(data) == data.length()) {
    file.close();
    return true;
  }
  file.close();
  return false;
}

String readFromSD(const String &filename) {
  if (!SD.exists(filename)) {
    return "";
  }

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading: " + filename);
    return "";
  }

  String content = file.readString();
  file.close();
  return content;
}

bool appendToSD(const String &filename, const String &data) {
  File file = SD.open(filename, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending: " + filename);
    return false;
  }

  if (file.print(data) == data.length()) {
    file.close();
    return true;
  }
  file.close();
  return false;
}

bool deleteFromSD(const String &filename) {
  return SD.remove(filename);
}

// ===== WiFi Configuration =====
struct WiFiConfig {
  String ssid;
  String password;
  String ntpServer = "pool.ntp.org";  // Default value
};


bool saveWiFiConfig(const WiFiConfig &config) {
  String data = "ssid=" + config.ssid + "\n";
  data += "password=" + config.password + "\n";
  data += "ntp=" + config.ntpServer + "\n";
  return saveToSD("/wifi_config.txt", data);
}

WiFiConfig loadWiFiConfig() {
  WiFiConfig config;
  config.ntpServer = "pool.ntp.org";  // Default
  String content = readFromSD("/wifi_config.txt");
  if (content.length() == 0) {
    return config;
  }

  int ssidPos = content.indexOf("ssid=");
  int passPos = content.indexOf("password=");
  int ntpPos = content.indexOf("ntp=");

  if (ssidPos != -1) {
    int endPos = content.indexOf('\n', ssidPos);
    config.ssid = content.substring(ssidPos + 5, endPos);
  }

  if (passPos != -1) {
    int endPos = content.indexOf('\n', passPos);
    config.password = content.substring(passPos + 9, endPos);
  }

  if (ntpPos != -1) {
    int endPos = content.indexOf('\n', ntpPos);
    config.ntpServer = content.substring(ntpPos + 4, endPos);
  }

  return config;
}

// ===== Fingerprint Database =====
bool saveFingerprintDB() {
  String data;

  for (int id = 1; id <= 300; id++) {
    if (finger.loadModel(id) == FINGERPRINT_OK) {
      data += String(id) + "," + getNameByID(id) + "\n";
    }
  }

  return saveToSD("/fingerprint_db.csv", data);
}

void loadFingerprintDB() {
  String content = readFromSD("/fingerprint_db.csv");
  if (content.length() == 0) return;

  int startPos = 0;
  while (startPos < content.length()) {
    int endPos = content.indexOf('\n', startPos);
    if (endPos == -1) endPos = content.length();

    String line = content.substring(startPos, endPos);
    int commaPos = line.indexOf(',');

    if (commaPos != -1) {
      int id = line.substring(0, commaPos).toInt();
      String name = line.substring(commaPos + 1);
      // Store name-ID mapping as needed
    }

    startPos = endPos + 1;
  }
}

void logAttendance(int fingerID, const String &name) {
  DateTime now = rtc.now();

  // Create ISO 8601 formatted timestamp
  String timestamp = String(now.year()) + "-" + (now.month() < 10 ? "0" : "") + String(now.month()) + "-" + (now.day() < 10 ? "0" : "") + String(now.day()) + " " + (now.hour() < 10 ? "0" : "") + String(now.hour()) + ":" + (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" + (now.second() < 10 ? "0" : "") + String(now.second());

  // Save to comprehensive local log
  String localEntry = String(fingerID) + "," + name + "," + timestamp + "\n";
  if (!appendToSD("/attendance.csv", localEntry)) {
    Serial.println("Error saving to local log");
  }

  // Save to pending sync queue (minimal data)
  String pendingEntry = String(fingerID) + "," + timestamp + "\n";
  if (!appendToSD(PENDING_ATTENDANCE_FILE, pendingEntry)) {
    Serial.println("Error saving to pending queue");
  }

  Serial.printf("Logged: ID %d at %s\n", fingerID, timestamp.c_str());
}

String getAttendanceLogs(int maxEntries = 10) {
  String content = readFromSD("/attendance.csv");
  if (content.length() == 0) return "";

  String result;
  int count = 0;
  int startPos = content.length() - 1;

  // Read backwards to get most recent entries
  while (startPos >= 0 && count < maxEntries) {
    int lineStart = content.lastIndexOf('\n', startPos - 1);
    if (lineStart == -1) lineStart = 0;

    String line = content.substring(lineStart, startPos);
    if (line.length() > 1) {
      result = line + "\n" + result;
      count++;
    }

    startPos = lineStart - 1;
    if (startPos < 0) break;
  }

  return result;
}

// ===== Configuration Mode =====
void enterConfigMode() {
  WiFi.disconnect(true);
  delay(1000);

  // String apName = "ChekinPlus-Config-" + String(ESP.getEfuseMac(), HEX);
  String apName = "ChekinPlus-Config";
  WiFi.softAP(apName.c_str());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", handleSaveSD);
  server.onNotFound(handleRoot);
  server.begin();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("CONFIG MODE");
  display.setCursor(0, 16);
  display.print("SSID: ");
  display.println(apName);
  display.setCursor(0, 32);
  display.println("IP: 192.168.4.1");
  display.display();

  unsigned long configStartTime = millis();
  const unsigned long timeout = 60000;  // 1 minutes

  while (millis() - configStartTime < timeout) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(10);

    if (server.hasArg("ssid")) break;
  }

  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
}

void handleRoot() {
  String html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>ChekinPlus Configuration</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial; margin: 20px; }
        .container { max-width: 400px; margin: 0 auto; }
        input { width: 100%; padding: 10px; margin: 8px 0; }
        button { background: #0066cc; color: white; padding: 10px; border: none; width: 100%; }
      </style>
    </head>
    <body>
      <div class="container">
        <h2>ChekinPlus Setup</h2>
        <form action="/save" method="post">
          <label for="ssid">WiFi SSID:</label>
          <input type="text" id="ssid" name="ssid" required>
          
          <label for="password">WiFi Password:</label>
          <input type="password" id="password" name="password">
          
          <label for="ntp">NTP Server:</label>
          <input type="text" id="ntp" name="ntp" value="pool.ntp.org">
          
          <button type="submit">Save Settings</button>
        </form>
      </div>
    </body>
    </html>
    )=====";

  server.send(200, "text/html", html);
}

void handleSaveSD() {
  if (server.hasArg("ssid")) {
    WiFiConfig config;
    config.ssid = server.arg("ssid");
    config.password = server.arg("password");
    config.ntpServer = server.hasArg("ntp") ? server.arg("ntp") : "pool.ntp.org";

    if (saveWiFiConfig(config)) {
      String html = R"=====(
        <!DOCTYPE html>
        <html><head>
          <meta http-equiv="refresh" content="5;url=/">
          <title>Settings Saved</title>
        </head>
        <body>
          <h2>Settings Saved to Device!</h2>
          <p>Device will restart in 5 seconds...</p>
        </body></html>
        )=====";

      server.send(200, "text/html", html);
      delay(1000);
      ESP.restart();
    } else {
      server.send(500, "text/plain", "Save to Device Error!");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// ===== WiFi Functions =====
void initWiFi(const WiFiConfig &config) {
  if (config.ssid.length() == 0) {
    Serial.println("No WiFi credentials configured");
    return;
  }

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());

  Serial.print("Connecting to WiFi");
  unsigned long startTime = millis();
  const unsigned long timeout = 30000;  // 30 second timeout

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
    delay(500);
    Serial.print(".");

    if (millis() - startTime > 10000 && WiFi.status() == WL_NO_SSID_AVAIL) {
      Serial.println("\nNetwork not found");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nConnected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Configure time with the saved NTP server
    if (ntpServer && ntpServer != "pool.ntp.org") {
      free((void *)ntpServer);
    }
    ntpServer = strdup(config.ntpServer.c_str());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("\nConnection failed!");
  }
}

void maintainWiFi() {
  static unsigned long lastCheck = 0;
  const unsigned long checkInterval = 30000;  // 30 seconds

  if (millis() - lastCheck > checkInterval) {
    lastCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      WiFi.disconnect();
      delay(100);
      WiFi.reconnect();
      delay(100);
    } else if (!wifiConnected) {
      wifiConnected = true;
      Serial.println("WiFi reconnected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
}

// ===== Interrupt Handlers =====
void IRAM_ATTR onFingerTouch() {
  fingerTouched = true;
}
void IRAM_ATTR onButtonPress() {
  if (!buttonBeingHandled) buttonPressedFlag = true;
}

bool getToken(const String &device_id, const String &default_token) {
  Serial.println("[AUTH] Attempting to get token...");

  HTTPClient http;
  http.setTimeout(10000);

  String url = base_url + "/attendify/api/get_token";
  String payload = "{\"dev_id\":\"" + device_id + "\",\"p_token\":\"" + default_token + "\"}";
  
  Serial.print("[AUTH] URL: ");
  Serial.println(url);
  Serial.print("[AUTH] Payload: ");
  Serial.println(payload);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);
  
  // PRINT HTTP CODE AND RESPONSE INFO
  Serial.print("[AUTH] HTTP Response Code: ");
  Serial.println(httpCode);
  
  bool success = false;

  if (httpCode == HTTP_CODE_OK) {
    JsonDocument doc;
    String response = http.getString();
    
    Serial.print("[AUTH] Response: ");
    Serial.println(response);
    
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print("[AUTH] JSON Deserialization Failed: ");
      Serial.println(error.c_str());
    } else {
      // Use the new syntax instead of containsKey
      if (doc["token"].is<String>()) {
        auth_token = doc["token"].as<String>();
        success = true;
        Serial.println("[AUTH] Got token from 'token' field");
      } else if (doc["o_token"].is<String>()) {
        auth_token = doc["o_token"].as<String>();
        success = true;
        Serial.println("[AUTH] Got token from 'o_token' field");
      } else {
        Serial.println("[AUTH] No token field found in response");
        // Print all available keys for debugging
        Serial.println("[AUTH] Available keys in response:");
        for (JsonPair kv : doc.as<JsonObject>()) {
          Serial.print("  - ");
          Serial.println(kv.key().c_str());
        }
      }

      if (success) {
        Serial.println("[AUTH] Success! Token: " + auth_token);
        if (saveAuthToken(auth_token)) {
          Serial.println("[AUTH] Token saved to SD card");
        } else {
          Serial.println("[AUTH] Failed to save token to SD card");
        }
      }
    }
  } else {
    // Print error details for non-200 responses
    Serial.print("[AUTH] HTTP Error: ");
    Serial.println(httpCode);
    
    String errorResponse = http.getString();
    if (errorResponse.length() > 0) {
      Serial.print("[AUTH] Error Response: ");
      Serial.println(errorResponse);
    }
    
    // Print WiFi status if connection failed
    if (httpCode < 0) {
      Serial.print("[AUTH] WiFi Status: ");
      Serial.println(WiFi.status());
      Serial.print("[AUTH] WiFi Connected: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "Yes" : "No");
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[AUTH] IP Address: ");
        Serial.println(WiFi.localIP());
      }
    }
  }

  http.end();
  return success;
}

bool parseJSON(HTTPClient &http, JsonDocument &doc) { // Changed parameter type
  Serial.println("Start parseJSON");
  String response = http.getString();
  Serial.println("Raw response: " + response);

  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return false;
  }
  return true;
}

bool getData(const String &device_id, const String &cid, uint8_t retryCount = 2) {
  // Try loaded token first
  if (auth_token.isEmpty()) {
    auth_token = loadAuthToken();
    if (!auth_token.isEmpty()) {
      Serial.println("[DATA] Using stored token from SD card");
    } else {
      Serial.println("[DATA] No stored token found");
    }
  }

  HTTPClient http;
  JsonDocument doc;
  bool success = false;

  String url = base_url + "/attendify/api/get_data";
  String payload = "{\"dev_id\":\"" + device_id + "\",\"o_token\":\"" + auth_token + "\",\"cid\":\"" + cid + "\"}";
  
  Serial.println("[DATA] === Starting getData Request ===");
  Serial.print("[DATA] URL: ");
  Serial.println(url);
  Serial.print("[DATA] Payload: ");
  Serial.println(payload);
  Serial.print("[DATA] Token length: ");
  Serial.println(auth_token.length());
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  int httpCode = http.POST(payload);
  
  // PRINT HTTP CODE AND RESPONSE INFO
  Serial.print("[DATA] HTTP Response Code: ");
  Serial.println(httpCode);
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.print("[DATA] Response: ");
    Serial.println(response);
    
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
      Serial.print("[DATA] JSON Deserialization Failed: ");
      Serial.println(error.c_str());
    } else {
      Serial.println("[DATA] Received valid data");
      success = true;
      
      // Print received data structure for debugging
      Serial.println("[DATA] Received JSON structure:");
      serializeJsonPretty(doc, Serial);
      Serial.println();
    }
  } 
  else if (httpCode == HTTP_CODE_UNAUTHORIZED) {
    Serial.println("[DATA] ❌ Token expired or invalid");
    String response = http.getString();
    if (response.length() > 0) {
      Serial.print("[DATA] Error response: ");
      Serial.println(response);
    }
    
    if (retryCount > 0) {
      Serial.println("[DATA] 🔄 Getting new token and retrying...");
      clearAuthToken();  // Remove invalid token
      http.end();
      if (getToken(device_id, default_token)) {
        return getData(device_id, cid, retryCount - 1);  // Retry with new token
      } else {
        Serial.println("[DATA] ❌ Failed to get new token");
      }
    } else {
      Serial.println("[DATA] ❌ Max retries reached");
    }
  }
  else {
    // Handle other HTTP errors
    Serial.print("[DATA] ❌ HTTP Error: ");
    Serial.println(httpCode);
    
    String errorResponse = http.getString();
    if (errorResponse.length() > 0) {
      Serial.print("[DATA] Error response: ");
      Serial.println(errorResponse);
    }
    
    // Print WiFi status if connection failed
    if (httpCode < 0) {
      Serial.print("[DATA] WiFi Status: ");
      Serial.println(WiFi.status());
      Serial.print("[DATA] WiFi Connected: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "Yes" : "No");
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[DATA] IP Address: ");
        Serial.println(WiFi.localIP());
      }
    }
  }

  http.end();
  Serial.print("[DATA] Request result: ");
  Serial.println(success ? "SUCCESS" : "FAILED");
  Serial.println("[DATA] === End getData Request ===\n");
  return success;
}

void loadAndValidateToken() {
  Serial.println("\n" + String(50, '='));
  Serial.println("=== TOKEN VALIDATION PROCESS START ===");
  Serial.println(String(50, '='));
  
  // 1. Load stored token
  Serial.println("[TOKEN] Step 1: Loading stored token from SD card...");
  auth_token = loadAuthToken();
  
  if (!auth_token.isEmpty()) {
    Serial.print("✅ Token loaded: ");
    Serial.println(auth_token.substring(0, 10) + "..."); // Show first 10 chars for security
    Serial.print("📏 Token length: ");
    Serial.println(auth_token.length());
    
    // 2. Test if stored token works
    Serial.println("\n[TOKEN] Step 2: Validating stored token...");
    Serial.println("Sending test request to verify token...");
    
    if (getData(device_id, cid, 0)) {
      Serial.println("🎉 SUCCESS: Stored token is valid!");
      Serial.println("✅ Using existing token");
      Serial.println(String(50, '='));
      Serial.println("=== TOKEN VALIDATION COMPLETE ===\n");
      return;
    } else {
      Serial.println("❌ FAILED: Stored token is invalid or expired");
      Serial.println("🗑️ Clearing invalid token...");
      clearAuthToken();
      auth_token = "";
    }
  } else {
    Serial.println("❌ No stored token found on SD card");
  }

  // 3. Get new token
  Serial.println("\n[TOKEN] Step 3: Requesting new token from server...");
  Serial.print("📡 Server: ");
  Serial.println(base_url);
  Serial.print("🆔 Device ID: ");
  Serial.println(device_id);
  
  if (getToken(device_id, default_token)) {
    Serial.println("✅ SUCCESS: New token received from server");
    Serial.print("📏 New token length: ");
    Serial.println(auth_token.length());
    
    // 4. Verify new token works
    Serial.println("\n[TOKEN] Step 4: Validating new token...");
    if (getData(device_id, cid, 0)) {
      Serial.println("🎉 SUCCESS: New token validated and working!");
      Serial.println("✅ System ready with new token");
    } else {
      Serial.println("❌ CRITICAL: New token validation failed!");
      Serial.println("🚨 This indicates a server or configuration issue");
      Serial.println("💡 Possible causes:");
      Serial.println("   - Server not responding properly");
      Serial.println("   - Device ID not recognized");
      Serial.println("   - Network connectivity issues");
      Serial.println("   - Server API changes");
      
      // Clear the potentially bad token
      clearAuthToken();
      auth_token = "";
    }
  } else {
    Serial.println("❌ CRITICAL: Failed to get new token from server!");
    Serial.println("🚨 System cannot authenticate with server");
    Serial.println("💡 Possible causes:");
    Serial.println("   - Network connectivity issues");
    Serial.println("   - Invalid device ID or default token");
    Serial.println("   - Server down or unreachable");
    Serial.println("   - Incorrect server URL");
    
    // Enter fallback mode - you could add config mode here
    Serial.println("🔧 Consider entering configuration mode...");
  }

  Serial.println(String(50, '='));
  Serial.println("=== TOKEN VALIDATION PROCESS END ===");
  Serial.println(String(50, '=') + "\n");
}


// Save a pending attendance record
bool savePendingAttendance(const String &record) {
  if (!appendToSD(PENDING_ATTENDANCE_FILE, record + "\n")) {
    Serial.println("Failed to save pending attendance");
    return false;
  }
  return true;
}

// Get count of pending records
int getPendingRecordCount() {
  String content = readFromSD(PENDING_ATTENDANCE_FILE);
  if (content.length() == 0) return 0;

  int count = 0;
  int pos = 0;
  while ((pos = content.indexOf('\n', pos)) != -1) {
    count++;
    pos++;  // Move past the newline
  }
  return count;
}
void processPendingAttendances() {
  static unsigned long lastSyncAttempt = 0;
  static unsigned long retryDelay = SYNC_INTERVAL;

  // Check if it's time to attempt sync
  if (millis() - lastSyncAttempt < retryDelay) {
    return;
  }
  lastSyncAttempt = millis();

  // Skip if WiFi isn't connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping sync - WiFi disconnected");
    retryDelay = SYNC_RETRY_DELAY;
    return;
  }

  // Load pending records
  String pendingData = readFromSD(PENDING_ATTENDANCE_FILE);
  if (pendingData.isEmpty()) {
    Serial.println("No pending records to sync");
    retryDelay = SYNC_INTERVAL;
    return;
  }

  Serial.println("Starting attendance sync...");
  Serial.print("Pending data: ");
  Serial.println(pendingData);

  // Prepare JSON document matching the Postman format
  JsonDocument doc; // Changed from DynamicJsonDocument
  doc["dev_id"] = device_id;
  doc["o_token"] = auth_token;
  doc["cid"] = cid;
  
  JsonArray records = doc["attendance_records"].to<JsonArray>();

  // Process records line by line
  int recordsProcessed = 0;
  int startPos = 0;

  while (startPos < pendingData.length()) {
    int endPos = pendingData.indexOf('\n', startPos);
    if (endPos == -1) endPos = pendingData.length();

    // FIX: Proper string trimming
    String line = pendingData.substring(startPos, endPos);
    line.trim(); // Call trim() separately
    
    if (line.length() > 0) {
      int commaPos = line.indexOf(',');
      if (commaPos != -1) {
        String emp_id = line.substring(0, commaPos);
        String timestamp = line.substring(commaPos + 1);
        
        // Convert timestamp to ISO 8601 format with T and Z
        timestamp.replace(" ", "T");
        timestamp += "Z";

        // Add to JSON array
        JsonObject record = records.add<JsonObject>();
        record["emp_id"] = emp_id;
        record["timestamp"] = timestamp;
        recordsProcessed++;
        
        Serial.printf("Added record: emp_id=%s, timestamp=%s\n", emp_id.c_str(), timestamp.c_str());
      }
    }
    startPos = endPos + 1;
  }

  if (recordsProcessed == 0) {
    Serial.println("No valid records to sync");
    return;
  }

  Serial.printf("Attempting to sync %d records\n", recordsProcessed);

  // Debug: Print the JSON before sending
  Serial.println("=== Final JSON to send ===");
  String debugPayload;
  serializeJsonPretty(doc, debugPayload);
  Serial.println(debugPayload);
  Serial.println("==========================");

  // Attempt to send all records
  bool success = sendAttendanceRecords(doc);

  // Update pending records file based on success
  if (success) {
    Serial.printf("Successfully synced %d records\n", recordsProcessed);
    
    // Clear the pending file after successful sync
    if (deleteFromSD(PENDING_ATTENDANCE_FILE)) {
      Serial.println("Pending file cleared after successful sync");
    }
    retryDelay = SYNC_INTERVAL;
  } else {
    Serial.println("Sync failed - will retry later");
    retryDelay = SYNC_RETRY_DELAY;
  }
}

bool sendAttendanceRecords(JsonDocument &doc) { // Changed parameter type
  // Validate token first
  if (auth_token.isEmpty()) {
    if (!getToken(device_id, default_token)) {
      Serial.println("Failed to get auth token");
      return false;
    }
    // Update the token in the document
    doc["o_token"] = auth_token;
  }

  HTTPClient http;
  String url = base_url + "/attendify/api/send_attendance";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000); // Increased timeout

  String payload;
  serializeJson(doc, payload);
  Serial.print("Sending payload: ");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  bool success = false;

  Serial.printf("HTTP Response code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    success = true;
    Serial.println("All records synced successfully");
    
    // Print server response
    String response = http.getString();
    Serial.print("Server response: ");
    Serial.println(response);
  } 
  else if (httpCode == HTTP_CODE_UNAUTHORIZED) {
    Serial.println("Token expired - refreshing...");
    
    // Clear invalid token
    clearAuthToken();
    auth_token = "";
    
    // Get new token
    if (getToken(device_id, default_token)) {
      // Update token in the document
      doc["o_token"] = auth_token;
      
      http.end(); // Close previous connection
      
      // Retry with new token
      return sendAttendanceRecords(doc); // Recursive call with new token
    }
  } 
  else {
    String response = http.getString();
    Serial.printf("Sync failed: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
    Serial.print("Server response: ");
    Serial.println(response);
    
    // Print detailed error information
    if (httpCode == 500) {
      Serial.println("Server error 500 - Check server logs");
    }
  }

  http.end();
  return success;
}
// ===== Setup Function =====
void setup() {
  Serial.begin(115200);

  // Initialize hardware
  Wire.begin(I2C_SDA, I2C_SCL);
  display.begin(0x3C, true);

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor((128 - 5 * 18) / 2, 20);
  display.print("Starting");
  display.display();
  // showCountdown();

  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    display.clearDisplay();
    display.println("Storage!");
    display.display();
    // while(1);
  }

  // Load fingerprint database
  loadFingerprintDB();

  // Initialize WiFi from SD card
  WiFiConfig config = loadWiFiConfig();
  if (config.ssid.length() > 0) {
    initWiFi(config);
  } else {
    enterConfigMode();
  }


  // Initialize GPIOs
  pinMode(TOUCH_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), onFingerTouch, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPress, FALLING);

  // Initialize RTC
  if (!rtc.begin()) {
    display.clearDisplay();
    display.println("Time Error!");
    display.display();
    // while(1);
  }
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Initialize Fingerprint
  if (!finger.verifyPassword()) {
    display.clearDisplay();
    display.println("Sensor error!");
    display.display();
    // while(1);
  }
  loadAndValidateToken();
  display.clearDisplay();
  showCountdown();
}

// ===== Main Loop =====
void loop() {
  maintainWiFi();
  processPendingAttendances();
  if (menuMode) {
    handleMenuNavigation();
    return;
  }

  // Normal operation
  updateDisplay();

  if (fingerTouched) {
    fingerTouched = false;
    delay(200);
    checkAttendance();
    delay(1000);
  }
  if (buttonPressedFlag && !menuMode) {
    enterMenuMode();
    return;
  }
  syncRTCTime();
}

// ===== Menu Handling Functions =====
void handleMenuNavigation() {
  if (millis() - lastMenuInteraction > menuTimeout) {
    exitMenuMode();
    return;
  }

  if (buttonPressedFlag) {
    pressStartTime = millis();
    buttonBeingHandled = true;
    buttonPressedFlag = false;
  }

  if (buttonBeingHandled && digitalRead(BUTTON_PIN) == HIGH) {
    unsigned long pressDuration = millis() - pressStartTime;
    lastMenuInteraction = millis();

    if (pressDuration >= LONG_PRESS_THRESHOLD) {
      executeMenuAction();
    } else {
      navigateMenu();
    }
    buttonBeingHandled = false;
  }
}

void enterMenuMode() {
  menuMode = true;
  buttonPressedFlag = false;
  lastMenuInteraction = millis();
  menuStartTime = millis();
  currentMenu = 0;
  showButtonMenu();
  Serial.println("📋 Menu mode activated");
  delay(100);
}

void exitMenuMode() {
  Serial.println("⏳ Menu timeout - exiting.");
  menuMode = false;
  currentMenu = 0;
  display.clearDisplay();
  display.display();
  delay(100);
}

void executeMenuAction() {
  Serial.print("Selected: ");
  Serial.println(menuItems[currentMenu]);
  switch (currentMenu) {
    case 0:  // Register Finger
      enrollFingerprint();
      break;
    case 1:  // Delete Finger
      startDeleteFingerprintProcess();
      break;
    case 2:
      listFingerprints();
      break;
    case 3:
      showLastLogs();
      break;
    case 4:
      enterConfigMode();
      break;
    case 5:  // Exit
      Serial.println("Exiting menu");
      break;
  }

  if (currentMenu != 5) {
    buzzerSuccess();
    delay(500);
    showButtonMenu();
  } else {
    exitMenuMode();
  }
}

void navigateMenu() {
  currentMenu = (currentMenu + 1) % menuCount;
  buzzerFail();
  showButtonMenu();
}

// ===== Time Synchronization =====
void syncRTCTime() {
  if (wifiConnected) {
    static unsigned long lastRtcSync = 0;
    const unsigned long syncInterval = 3600000;  // 1 hour

    if (millis() - lastRtcSync > syncInterval || lastRtcSync == 0) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        rtc.adjust(DateTime(
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec));
        Serial.println("✅ RTC updated from NTP.");
        lastRtcSync = millis();
      }
    }
  }
}

// ===== Display Functions =====
void showButtonMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  for (int i = 0; i < menuCount; i++) {
    display.setCursor(10, 0 + i * 12);
    if (i == currentMenu) {
      display.print(">");
    } else {
      display.print(" ");
    }
    display.print(menuItems[i]);
  }
  display.display();
}

void showCountdown() {
  for (int i = 5; i > 0; i--) {
    display.clearDisplay();
    display.setTextSize(5);
    display.setCursor(50, 20);
    display.print(i);
    display.display();
    delay(1000);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor((128 - 5 * 24) / 2, 20);
  display.print("ChekinPlus");
  display.display();
  delay(1000);
  display.setTextSize(1);
}

void updateDisplay() {
  display.clearDisplay();

  // Top Left: Device ID
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("FP2025070001");

  // Top Right: WiFi Status
  int wifiX = 128 - 16 - 2;
  if (WiFi.status() == WL_CONNECTED) {
    display.drawBitmap(wifiX, 0, wifi_connected_icon, 16, 16, SH110X_WHITE);
  } else {
    display.setTextSize(2);
    int textWidth = 6 * 2;
    display.drawBitmap(wifiX, 0, wifi_connected_icon, 16, 16, SH110X_WHITE);
    display.setCursor(128 - textWidth - 10, 0);
    display.print("!");
    display.setTextSize(1);
  }

  // Middle: "Ready" or connection status
  display.setTextSize(2);
  display.setCursor((128 - 5 * 24) / 2, 20);
  display.print("ChekinPlus");

  // Time display
  DateTime now = rtc.now();
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  display.setTextSize(2);
  display.setCursor((128 - 6 * 8 * 2) / 2, 40);
  display.print(timeStr);

  display.setTextSize(1);
  // Scrolling website
  int textWidth = strlen(websiteText) * 6;
  if (millis() - lastScrollTime > scrollDelay) {
    scrollPosition--;
    if (scrollPosition < -textWidth) scrollPosition = 128;
    lastScrollTime = millis();
  }
  display.setCursor(scrollPosition, 56);
  display.print(websiteText);

  display.display();
}


// ===== Fingerprint Functions =====
void checkAttendance() {
  const int maxRetries = 2;  // Number of silent retry attempts
  bool success = false;
  
  display.fillRect(0, 32, 128, 32, SH110X_BLACK);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor((128 - 6 * 2 * 10) / 2, 20);
  display.print("Scanning...");
  display.display();

  // Attempt fingerprint recognition with silent retries
  for (int attempt = 1; attempt <= maxRetries && !success; attempt++) {
    Serial.printf("Attempt %d/%d\n", attempt, maxRetries);
    
    // Step 1: Get fingerprint image
    if (finger.getImage() != FINGERPRINT_OK) {
      Serial.println("Finger not detected");
      delay(500); // Brief pause before retry
      continue;
    }

    // Step 2: Convert image to template
    if (finger.image2Tz() != FINGERPRINT_OK) {
      Serial.println("Image processing failed");
      delay(500);
      continue;
    }

    // Step 3: Search database
    if (finger.fingerSearch() == FINGERPRINT_OK) {
      success = true;
      recordAttendance();
    } else {
      Serial.println("Fingerprint not recognized");
      delay(500);
    }
  }

  // Final failure if all retries exhausted
  if (!success) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor((128 - 6 * 2 * 10) / 2, 20);
    display.print("Please Try");
    display.setCursor((128 - 6 * 2 * 5) / 2, 40);
    display.print("Again");
    display.display();
    buzzerFail();
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

void recordAttendance() {
  String name = getNameByID(finger.fingerID);
  DateTime now = rtc.now();
  
  char dateStr[11], timeStr[9];
  sprintf(dateStr, "%02d/%02d/%04d", now.day(), now.month(), now.year());
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  Serial.print("Success - ID: ");
  Serial.print(finger.fingerID);
  Serial.print(" Name: ");
  Serial.print(name);
  Serial.print(" Time: ");
  Serial.print(dateStr);
  Serial.print(" ");
  Serial.println(timeStr);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("ID ");
  display.print(finger.fingerID);
  display.setCursor(0, 16);
  display.print(name);
  display.setCursor(0, 32);
  display.print(dateStr);
  display.print(timeStr);
  display.setCursor(0, 24);
  display.display();
  buzzerSuccess();
  digitalWrite(BUZZER_PIN, LOW);
  delay(2000);

  logAttendance(finger.fingerID, name);
}

void startDeleteFingerprintProcess() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Scan Finger to Delete");
  display.display();
  delay(5000);
  if (finger.getImage() == FINGERPRINT_OK && finger.image2Tz() == FINGERPRINT_OK && finger.fingerSearch() == FINGERPRINT_OK) {
    deleteFingerprint(finger.fingerID);
  } else {
    failMessage("Scan failed");
  }
}

// ===== Utility Functions =====
void buzzerSuccess() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerFail() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

String getNameByID(int id) {
  switch (id) {
    case 1: return "Admin";
    case 2: return "Lion";
    case 3: return "Tanvir";
    case 4: return "Sadia";
    default: return "Unknown";
  }
}

void successMessage(String msg) {
  Serial.println("✅ " + msg);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor((128 - 6 * 2 * 8) / 2, 0);
  display.println("Success!");
  display.setCursor(0, 40);
  display.println(msg);
  display.display();
  buzzerSuccess();
  delay(2000);
}

void failMessage(String msg) {
  Serial.println("❌ " + msg);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor((128 - 6 * 2 * 7) / 2, 0);
  display.println("Failed!");
  display.setCursor(0, 40);
  display.println(msg);
  display.display();
  buzzerFail();
  delay(2000);
}

// ===== Fingerprint Management =====
int findNextAvailableID() {
  for (int id = 1; id <= 1000; id++) {
    if (finger.loadModel(id) != FINGERPRINT_OK) {
      return id;
    }
  }
  return -1;
}


void enrollFingerprint(int id) {
  // ID Validation
  if (id == -1) {
    id = findNextAvailableID();
    if (id == -1) {
      failMessage("Database full!");
      return;
    }
  } else if (id < 1 || id > 300) {
    failMessage("Invalid ID (1-300)");
    return;
  }

  // Initial Prompt
  display.clearDisplay();
  display.printf("Enrolling ID: %d", id);
  display.setCursor(0, 16);
  display.println("Place finger...");
  display.display();

  Serial.printf("Enrolling new fingerprint (ID: %d)\n", id);

  // First Capture
  if (!captureFingerprint(1, "Place finger...", "First scan done")) {
    return;
  }

  // Second Capture
  if (!captureFingerprint(2, "Place again...", "Second scan done")) {
    return;
  }

  // Check for duplicates BEFORE creating model
  if (finger.fingerFastSearch() == FINGERPRINT_OK) {
    failMessage("Finger already registered!");
    Serial.printf("❌ Already registered as ID: %d\n", finger.fingerID);
    return;
  }

  // Create Model
  display.println("Processing...");
  display.display();

  if (finger.createModel() != FINGERPRINT_OK) {
    failMessage("Prints don't match");
    return;
  }

  // Store Model
  if (finger.storeModel(id) == FINGERPRINT_OK) {
    successMessage("Stored as ID: " + String(id));
    saveFingerprintDB();
  } else {
    failMessage("Storage failed");
  }
}

bool captureFingerprint(int step, const char *prompt, const char *successMsg) {
  display.clearDisplay();
  display.printf("Step %d/2\n", step);
  display.println(prompt);
  display.display();

  unsigned long start = millis();
  bool waiting = true;

  // Improved scanning with visual feedback
  while (waiting) {
    int result = finger.getImage();

    if (result == FINGERPRINT_OK) {
      break;
    } else if (millis() - start > 10000) {
      failMessage("Timeout");
      return false;
    }

    // Show scanning animation
    static uint8_t dots = 0;
    display.setCursor(120, 0);
    display.print(".");
    dots = (dots + 1) % 4;
    display.display();
    delay(200);
  }

  if (finger.image2Tz(step) != FINGERPRINT_OK) {
    failMessage("Scan failed");
    return false;
  }

  Serial.println(successMsg);
  if (step == 1) {
    display.println("Remove finger");
    display.display();
    unsigned long removeStart = millis();
    while (finger.getImage() != FINGERPRINT_NOFINGER) {
      if (millis() - removeStart > 3000) {  // 3s remove timeout
        failMessage("Remove timeout");
        return false;
      }
      delay(100);
    }
  }
  return true;
}


void deleteFingerprint(int id) {
  if (id < 1 || id > 127) {
    Serial.println("❗ Invalid ID.");
    buzzerFail();
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
    return;
  }

  if (finger.deleteModel(id) == FINGERPRINT_OK) {
    Serial.print("🗑️ Deleted fingerprint ID: ");
    Serial.println(id);
    saveFingerprintDB();  // Update fingerprint database
  } else {
    Serial.println("❌ Deletion failed.");
    buzzerFail();
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

void listFingerprints() {
  if (finger.getTemplateCount() == FINGERPRINT_OK) {
    Serial.print("📂 Total fingerprints: ");
    Serial.println(finger.templateCount);
  } else {
    Serial.println("⚠️ Failed to get count.");
    buzzerFail();
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
    return;
  }

  Serial.print("✅ Found IDs: ");
  for (int id = 1; id <= 1000; id++) {
    if (finger.loadModel(id) == FINGERPRINT_OK) {
      Serial.print(id);
      Serial.print(" ");
      delay(50);
    }
  }
  Serial.println();
}

void showLastLogs() {
  String logs = getAttendanceLogs(5);
  if (logs.length() == 0) {
    Serial.println("⚠️ No attendance logs found.");
    buzzerFail();
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
    return;
  }

  Serial.println("📋 Last Attendance Logs:");
  Serial.println(logs);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Last Attendance:");

  int lineCount = 0;
  int startPos = 0;
  while (startPos < logs.length() && lineCount < 3) {
    int endPos = logs.indexOf('\n', startPos);
    if (endPos == -1) endPos = logs.length();

    String line = logs.substring(startPos, endPos);
    if (line.length() > 1) {
      display.setCursor(0, 16 + (lineCount * 16));
      display.println(line);
      lineCount++;
    }

    startPos = endPos + 1;
  }
  display.display();
}