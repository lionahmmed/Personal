#ifndef UTILITY_FUNCTIONS_H
#define UTILITY_FUNCTIONS_H

#include "config.h"
#include "globals.h"

// Basic Utility Functions
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

#endif