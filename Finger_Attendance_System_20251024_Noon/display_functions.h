#ifndef DISPLAY_FUNCTIONS_H
#define DISPLAY_FUNCTIONS_H

#include "config.h"
#include "globals.h"
#include "utility_functions.h"  // ADD THIS LINE

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

#endif