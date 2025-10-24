#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include "config.h"
#include "globals.h"
#include "display_functions.h"
#include "fingerprint_functions.h"
#include "wifi_functions.h"
#include "utility_functions.h"
#include "template_functions.h"  // ADD THIS

void exitMenuMode() {
  Serial.println("⏳ Menu timeout - exiting.");
  menuMode = false;
  currentMenu = 0;
  display.clearDisplay();
  display.display();
  delay(100);
}

void navigateMenu() {
  currentMenu = (currentMenu + 1) % menuCount;
  buzzerFail();
  showButtonMenu();
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
    case 4:  // List Fingerprints
      listFingerprints();
      break;
    case 5:  // Show Logs
      showLastLogs();
      break;
    case 2:  // Export Template (NEW)
      exportAllTemplates();
      break;
    case 3:  // Import Template (NEW)
      importTemplateFromFile();
      break;
    case 6:  // Set WiFi (moved from 4 to 6)
      enterConfigMode();
      break;
    case 7:  // Exit (moved from 5 to 7)
      Serial.println("Exiting menu");
      break;
  }

  if (currentMenu != 7) { // Updated from 5 to 7
    buzzerSuccess();
    delay(500);
    showButtonMenu();
  } else {
    exitMenuMode();
  }
}

void handleMenuNavigation() {
  static bool longPressExecuted = false;
  
  if (millis() - lastMenuInteraction > menuTimeout) {
    exitMenuMode();
    return;
  }
  
  if (buttonPressedFlag) {
    pressStartTime = millis();
    buttonBeingHandled = true;
    longPressExecuted = false;  // Reset flag on new press
    buttonPressedFlag = false;
  }

  if (buttonBeingHandled) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      // Button still pressed
      if (!longPressExecuted && (millis() - pressStartTime >= LONG_PRESS_THRESHOLD)) {
        // Execute once when threshold is first crossed
        executeMenuAction();
        lastMenuInteraction = millis();
        longPressExecuted = true;  // Prevent repeated execution
      }
    } else {
      // Button released
      unsigned long pressDuration = millis() - pressStartTime;
      lastMenuInteraction = millis();

      if (pressDuration < LONG_PRESS_THRESHOLD) {
        navigateMenu();
      }
      buttonBeingHandled = false;
      longPressExecuted = false;
    }
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

#endif