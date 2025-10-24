#ifndef SD_FUNCTIONS_H
#define SD_FUNCTIONS_H

#include "config.h"
#include "globals.h"
#include "utility_functions.h"  // ADD THIS LINE

// Basic SD Card Functions
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

// Token Management
bool saveAuthToken(const String &token) {
  return saveToSD(TOKEN_FILE, token);
}

String loadAuthToken() {
  return readFromSD(TOKEN_FILE);
}

bool clearAuthToken() {
  return deleteFromSD(TOKEN_FILE);
}

// WiFi Configuration
bool saveWiFiConfig(const WiFiConfig &config) {
  String data = "ssid=" + config.ssid + "\n";
  data += "password=" + config.password + "\n";
  data += "ntp=" + config.ntpServer + "\n";
  return saveToSD("/wifi_config.txt", data);
}

WiFiConfig loadWiFiConfig() {
  WiFiConfig config;
  config.ntpServer = "pool.ntp.org";
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

// Fingerprint Database
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
    }
    startPos = endPos + 1;
  }
}

// Attendance Logging
void logAttendance(int fingerID, const String &name) {
  DateTime now = rtc.now();
  String timestamp = String(now.year()) + "-" + (now.month() < 10 ? "0" : "") + String(now.month()) + "-" + (now.day() < 10 ? "0" : "") + String(now.day()) + " " + (now.hour() < 10 ? "0" : "") + String(now.hour()) + ":" + (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" + (now.second() < 10 ? "0" : "") + String(now.second());

  String localEntry = String(fingerID) + "," + name + "," + timestamp + "\n";
  if (!appendToSD("/attendance.csv", localEntry)) {
    Serial.println("Error saving to local log");
  }

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

// Pending Records
bool savePendingAttendance(const String &record) {
  if (!appendToSD(PENDING_ATTENDANCE_FILE, record + "\n")) {
    Serial.println("Failed to save pending attendance");
    return false;
  }
  return true;
}

int getPendingRecordCount() {
  String content = readFromSD(PENDING_ATTENDANCE_FILE);
  if (content.length() == 0) return 0;
  int count = 0;
  int pos = 0;
  while ((pos = content.indexOf('\n', pos)) != -1) {
    count++;
    pos++;
  }
  return count;
}

#endif