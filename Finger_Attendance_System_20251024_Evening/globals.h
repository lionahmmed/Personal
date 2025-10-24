#ifndef GLOBALS_H
#define GLOBALS_H

#include "config.h"
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

// Menu items
extern const char *menuItems[];
extern const int menuCount;

// WiFi icon
extern const uint8_t wifi_connected_icon[];

// Global Objects
extern Adafruit_SH1106G display;
extern RTC_DS3231 rtc;
extern HardwareSerial mySerial;
extern Adafruit_Fingerprint finger;

// Global Variables
extern String auth_token;
extern bool menuMode;
extern int currentMenu;
extern unsigned long menuStartTime;
extern unsigned long lastMenuInteraction;
extern volatile bool fingerTouched;
extern volatile bool buttonPressedFlag;
extern bool buttonBeingHandled;
extern unsigned long pressStartTime;
extern int scrollPosition;
extern unsigned long lastScrollTime;
extern const char *websiteText;
extern const int scrollDelay;
extern bool wifiConnected;
extern const char *ntpServer;

// Server objects
extern WebServer server;
extern DNSServer dnsServer;
extern const byte DNS_PORT;

#endif