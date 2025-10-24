#ifndef FINGERPRINT_FUNCTIONS_H
#define FINGERPRINT_FUNCTIONS_H

#include "config.h"
#include "globals.h"
#include "utility_functions.h"
#include "template_functions.h"  // ADD THIS'

int findNextAvailableID() {
  for (int id = 1; id <= 1000; id++) {
    if (finger.loadModel(id) != FINGERPRINT_OK) {
      return id;
    }
  }
  return -1;
}
bool captureFingerprint(int step, const char *prompt, const char *successMsg) {
  display.clearDisplay();
  display.printf("Step %d/2\n", step);
  display.println(prompt);
  display.display();

  unsigned long start = millis();
  bool waiting = true;

  while (waiting) {
    int result = finger.getImage();

    if (result == FINGERPRINT_OK) {
      break;
    } else if (millis() - start > 10000) {
      failMessage("Timeout");
      return false;
    }

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
      if (millis() - removeStart > 3000) {
        failMessage("Remove timeout");
        return false;
      }
      delay(100);
    }
  }
  return true;
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
void checkAttendance() {
  const int maxRetries = 2;
  bool success = false;
  
  display.fillRect(0, 32, 128, 32, SH110X_BLACK);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor((128 - 6 * 2 * 10) / 2, 20);
  display.print("Scanning...");
  display.display();

  for (int attempt = 1; attempt <= maxRetries && !success; attempt++) {
    Serial.printf("Attempt %d/%d\n", attempt, maxRetries);
    
    if (finger.getImage() != FINGERPRINT_OK) {
      Serial.println("Finger not detected");
      delay(500);
      continue;
    }

    if (finger.image2Tz() != FINGERPRINT_OK) {
      Serial.println("Image processing failed");
      delay(500);
      continue;
    }

    if (finger.fingerSearch() == FINGERPRINT_OK) {
      success = true;
      recordAttendance();
    } else {
      Serial.println("Fingerprint not recognized");
      delay(500);
    }
  }

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
bool saveFingerprintTemplate(int id, const String &name) {
  // Use the new function from template_functions.h
  String templateData = generateFingerprintID(id);
  
  if (templateData.isEmpty()) {
    Serial.println("Failed to generate fingerprint template");
    return false;
  }

  DateTime now = rtc.now();
  String timestamp = String(now.year()) + "-" + 
                    (now.month() < 10 ? "0" : "") + String(now.month()) + "-" + 
                    (now.day() < 10 ? "0" : "") + String(now.day()) + "T" + 
                    (now.hour() < 10 ? "0" : "") + String(now.hour()) + ":" + 
                    (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" + 
                    (now.second() < 10 ? "0" : "") + String(now.second()) + "Z";

  // Save to pending file: emp_id,timestamp,finger_id
  String record = String(id) + "," + timestamp + "," + templateData;
  
  // Use the template SD function
  File file = SD.open(PENDING_FINGERPRINTS_FILE, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending: " + PENDING_FINGERPRINTS_FILE);
    return false;
  }
  
  if (file.print(record + "\n") == (record.length() + 1)) {
    file.close();
    return true;
  }
  file.close();
  return false;
}
/*
void enrollFingerprint(int id = -1) {
  String name = "";
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

  display.clearDisplay();
  display.printf("Enrolling ID: %d", id);
  display.setCursor(0, 16);
  display.println("Place finger...");
  display.display();

  Serial.printf("Enrolling new fingerprint (ID: %d)\n", id);

  if (!captureFingerprint(1, "Place finger...", "First scan done")) {
    return;
  }

  if (!captureFingerprint(2, "Place again...", "Second scan done")) {
    return;
  }

  if (finger.fingerFastSearch() == FINGERPRINT_OK) {
    failMessage("Finger already registered!");
    Serial.printf("❌ Already registered as ID: %d\n", finger.fingerID);
    return;
  }

  display.println("Processing...");
  display.display();

  if (finger.createModel() != FINGERPRINT_OK) {
    failMessage("Prints don't match");
    return;
  }

  if (finger.storeModel(id) == FINGERPRINT_OK) {
    successMessage("Stored as ID: " + String(id));
    saveFingerprintDB();
    
    if (saveFingerprintTemplate(id, name)) {
      Serial.println("Fingerprint saved for server sync");
    } else {
      Serial.println("Failed to save fingerprint for server sync");
    }
  } else {
    failMessage("Storage failed");
  }
}*/

void enrollFingerprint(int id = -1) {
  String name = "";
  
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
  
  display.clearDisplay();
  display.printf("Enrolling ID: %d", id);
  display.setCursor(0, 16);
  display.println("Enter name...");
  display.display();
  
  // Prompt for name (you can enhance this with input method)
  Serial.println("Enter employee name:");
  unsigned long startTime = millis();
  while (!Serial.available() && millis() - startTime < 10000) {
    delay(100);
  }
  
  if (Serial.available()) {
    name = Serial.readStringUntil('\n');
    name.trim();
  }
  
  if (name.isEmpty()) {
    name = "Employee_" + String(id);
  }
  
  Serial.printf("Enrolling: ID=%d, Name=%s\n", id, name.c_str());
  
  display.clearDisplay();
  display.printf("ID: %d", id);
  display.setCursor(0, 16);
  display.println(name);
  display.setCursor(0, 32);
  display.println("Place finger...");
  display.display();
  
  // Capture first scan
  if (!captureFingerprint(1, "Place finger...", "First scan done")) {
    return;
  }
  
  // Capture second scan
  if (!captureFingerprint(2, "Place again...", "Second scan done")) {
    return;
  }
  
  // Check if already registered
  if (finger.fingerFastSearch() == FINGERPRINT_OK) {
    failMessage("Already registered!");
    Serial.printf("❌ Already registered as ID: %d\n", finger.fingerID);
    return;
  }
  
  display.println("Processing...");
  display.display();
  
  // Create model
  if (finger.createModel() != FINGERPRINT_OK) {
    failMessage("Prints don't match");
    return;
  }
  
  // Store in sensor
  if (finger.storeModel(id) == FINGERPRINT_OK) {
    successMessage("Stored as ID: " + String(id));
    saveFingerprintDB();
    
    // Capture and save template with full data for server sync
    if (captureAndSaveTemplateWithData(id, name)) {
      Serial.println("✅ Template queued for server upload");
      
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("Success!");
      display.setCursor(0, 20);
      display.println("Template will be");
      display.println("synced to server");
      display.display();
      buzzerSuccess();
      delay(2000);
    } else {
      Serial.println("⚠️ Failed to queue template for server");
    }
  } else {
    failMessage("Storage failed");
  }
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
    saveFingerprintDB();
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

#endif