#ifndef RTC_FUNCTIONS_H
#define RTC_FUNCTIONS_H

#include "config.h"
#include "globals.h"

void syncRTCTime() {
  if (wifiConnected) {
    static unsigned long lastRtcSync = 0;
    const unsigned long syncInterval = 3600000;

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

#endif