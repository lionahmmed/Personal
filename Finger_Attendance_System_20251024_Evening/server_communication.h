#ifndef SERVER_COMMUNICATION_H
#define SERVER_COMMUNICATION_H

#include "config.h"
#include "globals.h"
#include "sd_functions.h"

// ===== Server Communication Functions =====
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
    Serial.print("[AUTH] HTTP Error: ");
    Serial.println(httpCode);
    
    String errorResponse = http.getString();
    if (errorResponse.length() > 0) {
      Serial.print("[AUTH] Error Response: ");
      Serial.println(errorResponse);
    }
    
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

void processReceivedData(JsonDocument &doc) {
  Serial.println("[DATA] Processing received data...");
  
  if (doc["users"].is<JsonArray>()) {
    JsonArray users = doc["users"].as<JsonArray>();
    Serial.printf("[DATA] Received %d user records\n", users.size());
    
    for (JsonObject user : users) {
      if (user["emp_id"].is<String>() && user["name"].is<String>()) {
        String emp_id = user["emp_id"].as<String>();
        String name = user["name"].as<String>();
        Serial.printf("[DATA] User: %s - %s\n", emp_id.c_str(), name.c_str());
      }
    }
  }

  if (doc["config"].is<JsonObject>()) {
    JsonObject config = doc["config"].as<JsonObject>();
    Serial.println("[DATA] Received configuration updates");
    
    if (config["sync_interval"].is<unsigned long>()) {
      unsigned long newSyncInterval = config["sync_interval"].as<unsigned long>();
      Serial.printf("[DATA] New sync interval: %lu\n", newSyncInterval);
    }
    
    if (config["device_name"].is<String>()) {
      String deviceName = config["device_name"].as<String>();
      Serial.printf("[DATA] New device name: %s\n", deviceName.c_str());
    }
  }

  if (doc["fingerprints"].is<JsonArray>()) {
    JsonArray fingerprints = doc["fingerprints"].as<JsonArray>();
    Serial.printf("[DATA] Received %d fingerprint updates\n", fingerprints.size());
    
    for (JsonObject fp : fingerprints) {
      if (fp["emp_id"].is<String>() && fp["action"].is<String>()) {
        String emp_id = fp["emp_id"].as<String>();
        String action = fp["action"].as<String>();
        Serial.printf("[DATA] Fingerprint %s: %s\n", emp_id.c_str(), action.c_str());
      }
    }
  }

  if (doc["message"].is<String>()) {
    String message = doc["message"].as<String>();
    Serial.printf("[DATA] Server message: %s\n", message.c_str());
  }

  if (doc["firmware"].is<JsonObject>()) {
    JsonObject firmware = doc["firmware"].as<JsonObject>();
    if (firmware["update_available"].is<bool>() && firmware["update_available"].as<bool>()) {
      Serial.println("[DATA] Firmware update available!");
    }
  }

  if (doc["attendance_data"].is<JsonArray>()) {
    JsonArray attendance = doc["attendance_data"].as<JsonArray>();
    Serial.printf("[DATA] Received %d attendance records\n", attendance.size());
    
    for (JsonObject record : attendance) {
      if (record["emp_id"].is<String>() && record["timestamp"].is<String>()) {
        String emp_id = record["emp_id"].as<String>();
        String timestamp = record["timestamp"].as<String>();
        Serial.printf("[DATA] Attendance: %s at %s\n", emp_id.c_str(), timestamp.c_str());
      }
    }
  }

  Serial.println("[DATA] All received fields:");
  for (JsonPair kv : doc.as<JsonObject>()) {
    Serial.printf("  - %s: ", kv.key().c_str());
    if (kv.value().is<JsonArray>()) {
      Serial.printf("Array (%d items)\n", kv.value().as<JsonArray>().size());
    } else if (kv.value().is<JsonObject>()) {
      Serial.println("Object");
    } else {
      String value;
      serializeJson(kv.value(), value);
      Serial.println(value);
    }
  }
}

bool getData(const String &device_id, const String &cid, uint8_t retryCount = 2) {
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

  String url = base_url + "/attendify/api/get_data?api_key=eW7tTAfk1C";
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
      
      Serial.println("[DATA] Received JSON structure:");
      serializeJsonPretty(doc, Serial);
      Serial.println();

      if (doc["status"].is<String>()) {
        String status = doc["status"].as<String>();
        Serial.printf("[DATA] Server status: %s\n", status.c_str());
        
        if (status == "success") {
          Serial.println("[DATA] ✅ Server responded with success");
          processReceivedData(doc);
        } else if (status == "error") {
          Serial.println("[DATA] ⚠️ Server responded with error");
          if (doc["message"].is<String>()) {
            Serial.printf("[DATA] Error message: %s\n", doc["message"].as<String>().c_str());
          }
        }
      } else {
        Serial.println("[DATA] No status field, processing data anyway");
        processReceivedData(doc);
      }
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
      clearAuthToken();
      http.end();
      if (getToken(device_id, default_token)) {
        return getData(device_id, cid, retryCount - 1);
      } else {
        Serial.println("[DATA] ❌ Failed to get new token");
      }
    } else {
      Serial.println("[DATA] ❌ Max retries reached");
    }
  }
  else {
    Serial.print("[DATA] ❌ HTTP Error: ");
    Serial.println(httpCode);
    
    String errorResponse = http.getString();
    if (errorResponse.length() > 0) {
      Serial.print("[DATA] Error response: ");
      Serial.println(errorResponse);
    }
    
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
  
  Serial.println("[TOKEN] Step 1: Loading stored token from SD card...");
  auth_token = loadAuthToken();
  
  if (!auth_token.isEmpty()) {
    Serial.print("✅ Token loaded: ");
    Serial.println(auth_token.substring(0, 10) + "...");
    Serial.print("📏 Token length: ");
    Serial.println(auth_token.length());
    
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

  Serial.println("\n[TOKEN] Step 3: Requesting new token from server...");
  Serial.print("📡 Server: ");
  Serial.println(base_url);
  Serial.print("🆔 Device ID: ");
  Serial.println(device_id);
  
  if (getToken(device_id, default_token)) {
    Serial.println("✅ SUCCESS: New token received from server");
    Serial.print("📏 New token length: ");
    Serial.println(auth_token.length());
    
    Serial.println("\n[TOKEN] Step 4: Validating new token...");
    if (getData(device_id, cid, 0)) {
      Serial.println("🎉 SUCCESS: New token validated and working!");
      Serial.println("✅ System ready with new token");
    } else {
      Serial.println("❌ CRITICAL: New token validation failed!");
      Serial.println("🚨 This indicates a server or configuration issue");
      clearAuthToken();
      auth_token = "";
    }
  } else {
    Serial.println("❌ CRITICAL: Failed to get new token from server!");
    Serial.println("🚨 System cannot authenticate with server");
  }

  Serial.println(String(50, '='));
  Serial.println("=== TOKEN VALIDATION PROCESS END ===");
  Serial.println(String(50, '=') + "\n");
}

bool sendAttendanceRecords(JsonDocument &doc) {
  if (auth_token.isEmpty()) {
    if (!getToken(device_id, default_token)) {
      Serial.println("Failed to get auth token");
      return false;
    }
    doc["o_token"] = auth_token;
  }

  HTTPClient http;
  String url = base_url + "/attendify/api/send_attendance";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

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
    
    String response = http.getString();
    Serial.print("Server response: ");
    Serial.println(response);
  } 
  else if (httpCode == HTTP_CODE_UNAUTHORIZED) {
    Serial.println("Token expired - refreshing...");
    
    clearAuthToken();
    auth_token = "";
    
    if (getToken(device_id, default_token)) {
      doc["o_token"] = auth_token;
      
      http.end();
      return sendAttendanceRecords(doc);
    }
  } 
  else {
    String response = http.getString();
    Serial.printf("Sync failed: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
    Serial.print("Server response: ");
    Serial.println(response);
    
    if (httpCode == 500) {
      Serial.println("Server error 500 - Check server logs");
    }
  }

  http.end();
  return success;
}

void processPendingAttendances() {
  static unsigned long lastSyncAttempt = 0;
  static unsigned long retryDelay = SYNC_INTERVAL;

  if (millis() - lastSyncAttempt < retryDelay) {
    return;
  }
  lastSyncAttempt = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping sync - WiFi disconnected");
    retryDelay = SYNC_RETRY_DELAY;
    return;
  }

  String pendingData = readFromSD(PENDING_ATTENDANCE_FILE);
  if (pendingData.isEmpty()) {
    Serial.println("No pending records to sync");
    retryDelay = SYNC_INTERVAL;
    return;
  }

  Serial.println("Starting attendance sync...");
  Serial.print("Pending data: ");
  Serial.println(pendingData);

  JsonDocument doc;
  doc["dev_id"] = device_id;
  doc["o_token"] = auth_token;
  doc["cid"] = cid;
  
  JsonArray records = doc["attendance_records"].to<JsonArray>();

  int recordsProcessed = 0;
  int startPos = 0;

  while (startPos < pendingData.length()) {
    int endPos = pendingData.indexOf('\n', startPos);
    if (endPos == -1) endPos = pendingData.length();

    String line = pendingData.substring(startPos, endPos);
    line.trim();
    
    if (line.length() > 0) {
      int commaPos = line.indexOf(',');
      if (commaPos != -1) {
        String emp_id = line.substring(0, commaPos);
        String timestamp = line.substring(commaPos + 1);
        
        timestamp.replace(" ", "T");
        timestamp += "Z";

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

  Serial.println("=== Final JSON to send ===");
  String debugPayload;
  serializeJsonPretty(doc, debugPayload);
  Serial.println(debugPayload);
  Serial.println("==========================");

  bool success = sendAttendanceRecords(doc);

  if (success) {
    Serial.printf("Successfully synced %d records\n", recordsProcessed);
    
    if (deleteFromSD(PENDING_ATTENDANCE_FILE)) {
      Serial.println("Pending file cleared after successful sync");
    }
    retryDelay = SYNC_INTERVAL;
  } else {
    Serial.println("Sync failed - will retry later");
    retryDelay = SYNC_RETRY_DELAY;
  }
}

bool sendSingleFingerprintRecord(const String &emp_id, const String &finger_id) {
  JsonDocument doc;
  doc["cid"] = cid;
  doc["emp_id"] = emp_id;
  doc["name"] = "new_fp";
  doc["finger_id"] = finger_id;

  HTTPClient http;
  String url = base_url + "/attendify/api/send_new_fid?api_key=eW7tTAfk1C";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  String payload;
  serializeJson(doc, payload);
  
  Serial.println("=== Fingerprint JSON Payload ===");
  serializeJsonPretty(doc, Serial);
  Serial.println("\n=================================");

  int httpCode = http.POST(payload);
  bool success = false;

  Serial.printf("Fingerprint sync HTTP Response: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    success = true;
    Serial.println("Fingerprint record synced successfully");
    
    String response = http.getString();
    if (response.length() > 0) {
      Serial.print("Server response: ");
      Serial.println(response);
    }
  } 
  else if (httpCode == HTTP_CODE_UNAUTHORIZED) {
    Serial.println("Token expired during fingerprint sync");
    clearAuthToken();
    auth_token = "";
    
    if (getToken(device_id, default_token)) {
      http.end();
      return sendSingleFingerprintRecord(emp_id, finger_id);
    }
  } 
  else {
    String response = http.getString();
    Serial.printf("Fingerprint sync failed: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
    if (response.length() > 0) {
      Serial.print("Server response: ");
      Serial.println(response);
    }
  }

  http.end();
  return success;
}

void removeSentFingerprintRecords(String sentRecords[], int recordCount) {
  String allPendingData = readFromSD(PENDING_FINGERPRINTS_FILE);
  if (allPendingData.isEmpty()) {
    deleteFromSD(PENDING_FINGERPRINTS_FILE);
    return;
  }

  String remainingData = "";
  int startPos = 0;
  
  while (startPos < allPendingData.length()) {
    int endPos = allPendingData.indexOf('\n', startPos);
    if (endPos == -1) endPos = allPendingData.length();

    String line = allPendingData.substring(startPos, endPos);
    line.trim();
    
    bool wasSent = false;
    for (int i = 0; i < recordCount; i++) {
      if (line == sentRecords[i]) {
        wasSent = true;
        break;
      }
    }
    
    if (!wasSent && line.length() > 0) {
      remainingData += line + "\n";
    }
    
    startPos = endPos + 1;
  }

  if (remainingData.length() > 0) {
    saveToSD(PENDING_FINGERPRINTS_FILE, remainingData);
  } else {
    deleteFromSD(PENDING_FINGERPRINTS_FILE);
  }
}

bool sendNewFingerprintsToServer() {
  static unsigned long lastFingerprintSync = 0;
  const unsigned long syncInterval = 2 * 60 * 1000;

  if (millis() - lastFingerprintSync < syncInterval) {
    return false;
  }
  lastFingerprintSync = millis();

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  String pendingData = readFromSD(PENDING_FINGERPRINTS_FILE);
  if (pendingData.isEmpty()) {
    return true;
  }

  Serial.println("Starting fingerprint sync...");

  int recordsProcessed = 0;
  int startPos = 0;
  String processedRecords[MAX_FINGERPRINT_RECORDS];
  int recordCount = 0;

  while (startPos < pendingData.length() && recordCount < MAX_FINGERPRINT_RECORDS) {
    int endPos = pendingData.indexOf('\n', startPos);
    if (endPos == -1) endPos = pendingData.length();

    String line = pendingData.substring(startPos, endPos);
    line.trim();
    
    if (line.length() > 0) {
      int comma1 = line.indexOf(',');
      int comma2 = line.indexOf(',', comma1 + 1);

      if (comma1 != -1 && comma2 != -1) {
        String emp_id = line.substring(0, comma1);
        String timestamp = line.substring(comma1 + 1, comma2);
        String finger_id = line.substring(comma2 + 1);

        if (sendSingleFingerprintRecord(emp_id, finger_id)) {
          recordsProcessed++;
          processedRecords[recordCount] = line;
          recordCount++;
          Serial.printf("✅ Successfully sent fingerprint for emp_id: %s\n", emp_id.c_str());
        } else {
          Serial.printf("❌ Failed to send fingerprint for emp_id: %s\n", emp_id.c_str());
        }
      }
    }
    startPos = endPos + 1;
  }

  if (recordsProcessed == 0) {
    Serial.println("No valid fingerprint records to sync");
    return true;
  }

  Serial.printf("Successfully synced %d fingerprint records\n", recordsProcessed);
  
  removeSentFingerprintRecords(processedRecords, recordCount);
  
  return true;
}
// ============================================
// Add these functions to server_communication.h
// ============================================

// Send fingerprint template with actual binary data to server
bool sendFingerprintTemplateToServer(int emp_id, const String &name, 
                                      const String &finger_id, 
                                      const String &templateFile) {
  Serial.println("\n=== Sending Fingerprint Template ===");
  Serial.printf("Employee ID: %d\n", emp_id);
  Serial.printf("Name: %s\n", name.c_str());
  Serial.printf("Finger ID: %s\n", finger_id.c_str());
  Serial.printf("Template File: %s\n", templateFile.c_str());
  
  // Read template as Base64
  String templateBase64 = readTemplateAsBase64(templateFile);
  if (templateBase64.isEmpty()) {
    Serial.println("❌ Failed to read template data");
    return false;
  }
  
  // Get file size for metadata
  File file = SD.open(templateFile, FILE_READ);
  size_t templateSize = file.size();
  file.close();
  
  // Prepare JSON payload
  JsonDocument doc;
  doc["cid"] = cid;
  doc["dev_id"] = device_id;
  doc["emp_id"] = String(emp_id);
  doc["name"] = name;
  doc["finger_id"] = finger_id;
  doc["template_data"] = templateBase64;
  doc["template_size"] = templateSize;
  doc["template_format"] = "R307_RAW_BINARY";
  doc["encoding"] = "base64";
  
  // Add timestamp
  DateTime now = rtc.now();
  char timestamp[25];
  sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02dZ", 
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());
  doc["timestamp"] = timestamp;
  
  // Serialize to string
  String payload;
  serializeJson(doc, payload);
  
  Serial.println("\n=== Request Details ===");
  Serial.printf("Template Size: %d bytes\n", templateSize);
  Serial.printf("Base64 Length: %d chars\n", templateBase64.length());
  Serial.printf("Total Payload: %d bytes\n", payload.length());
  
  // Send to server
  HTTPClient http;
  String url = base_url + "/attendify/api/send_new_fid?api_key=eW7tTAfk1C";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);  // 30 second timeout for large data
  
  Serial.printf("\nSending to: %s\n", url.c_str());
  
  int httpCode = http.POST(payload);
  bool success = false;
  
  Serial.printf("HTTP Response Code: %d\n", httpCode);
  
  if (httpCode == HTTP_CODE_OK) {
    success = true;
    String response = http.getString();
    Serial.println("✅ Template sent successfully!");
    Serial.printf("Server Response: %s\n", response.c_str());
  } 
  else if (httpCode == HTTP_CODE_UNAUTHORIZED) {
    Serial.println("❌ Authentication failed - token may be expired");
    
    // Try to refresh token
    clearAuthToken();
    auth_token = "";
    
    if (getToken(device_id, default_token)) {
      http.end();
      Serial.println("🔄 Retrying with new token...");
      return sendFingerprintTemplateToServer(emp_id, name, finger_id, templateFile);
    }
  }
  else {
    String response = http.getString();
    Serial.printf("❌ Request failed: %d - %s\n", httpCode, 
                  http.errorToString(httpCode).c_str());
    if (response.length() > 0) {
      Serial.printf("Error Response: %s\n", response.c_str());
    }
  }
  
  http.end();
  Serial.println("===================================\n");
  
  return success;
}

// Process pending fingerprint templates and send to server
bool processPendingFingerprints() {
  static unsigned long lastSync = 0;
  const unsigned long syncInterval = 2 * 60 * 1000;  // 2 minutes
  
  if (millis() - lastSync < syncInterval) {
    return false;
  }
  lastSync = millis();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected - skipping fingerprint sync");
    return false;
  }
  
  String pendingData = readFromSD(PENDING_FINGERPRINTS_FILE);
  if (pendingData.isEmpty()) {
    return true;  // No pending data
  }
  
  Serial.println("\n📤 Starting fingerprint template sync...");
  
  int processed = 0;
  int succeeded = 0;
  String remainingData = "";
  
  int startPos = 0;
  while (startPos < pendingData.length()) {
    int endPos = pendingData.indexOf('\n', startPos);
    if (endPos == -1) endPos = pendingData.length();
    
    String line = pendingData.substring(startPos, endPos);
    line.trim();
    
    if (line.length() > 0) {
      processed++;
      
      // Parse: emp_id,name,timestamp,finger_id,template_file
      int comma1 = line.indexOf(',');
      int comma2 = line.indexOf(',', comma1 + 1);
      int comma3 = line.indexOf(',', comma2 + 1);
      int comma4 = line.indexOf(',', comma3 + 1);
      
      if (comma1 != -1 && comma2 != -1 && comma3 != -1 && comma4 != -1) {
        int emp_id = line.substring(0, comma1).toInt();
        String name = line.substring(comma1 + 1, comma2);
        String timestamp = line.substring(comma2 + 1, comma3);
        String finger_id = line.substring(comma3 + 1, comma4);
        String templateFile = line.substring(comma4 + 1);
        
        Serial.printf("\n[%d/%d] Processing: emp_id=%d, name=%s\n", 
                      processed, processed, emp_id, name.c_str());
        
        // Verify template file exists
        if (!SD.exists(templateFile)) {
          Serial.printf("⚠️ Template file not found: %s (skipping)\n", 
                       templateFile.c_str());
          continue;  // Skip this record
        }
        
        // Send to server
        if (sendFingerprintTemplateToServer(emp_id, name, finger_id, templateFile)) {
          succeeded++;
          Serial.println("✅ Successfully sent to server");
          
          // Optional: Delete template file after successful sync
          // SD.remove(templateFile);
        } else {
          Serial.println("❌ Failed to send - keeping in queue");
          remainingData += line + "\n";
        }
      } else {
        Serial.println("⚠️ Invalid record format - skipping");
      }
    }
    
    startPos = endPos + 1;
    delay(100);  // Small delay between requests
  }
  
  Serial.printf("\n📊 Sync Summary: %d processed, %d succeeded, %d failed\n", 
                processed, succeeded, processed - succeeded);
  
  // Update pending file with failed records
  if (remainingData.isEmpty()) {
    deleteFromSD(PENDING_FINGERPRINTS_FILE);
    Serial.println("✅ All fingerprints synced - queue cleared");
  } else {
    saveToSD(PENDING_FINGERPRINTS_FILE, remainingData);
    Serial.printf("⚠️ %d records remain in queue\n", 
                 processed - succeeded);
  }
  
  return (succeeded > 0);
}

#endif