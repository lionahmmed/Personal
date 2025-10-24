#include "config.h"
#include "globals.h"
#include "sd_functions.h"
#include "wifi_functions.h"
#include "display_functions.h"
#include "fingerprint_functions.h"
#include "server_communication.h"
#include "menu_system.h"
#include "rtc_functions.h"
#include "interrupts.h"

// Global object definitions
Adafruit_SH1106G display(128, 64, &Wire, -1);
RTC_DS3231 rtc;
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);

// Global variable definitions
String auth_token = "";
bool menuMode = false;
int currentMenu = 0;
unsigned long menuStartTime = 0;
unsigned long lastMenuInteraction = 0;
volatile bool fingerTouched = false;
volatile bool buttonPressedFlag = false;
bool buttonBeingHandled = false;
unsigned long pressStartTime = 0;
int scrollPosition = 0;
unsigned long lastScrollTime = 0;
const char *websiteText = "www.eonsystem.com";
const int scrollDelay = 150;
bool wifiConnected = false;

// Menu items
const char *menuItems[] = {
  "1. Register Finger",
  "2. Delete Finger", 
  "3. Export Template",
  "4. Import Template",  // ADD THIS
  "5. List Fingerprints",
  "6. Show Logs",
  "7. Set WiFi",
  "8. String device_idExit"
};
const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);


// WiFi objects
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
const char *ntpServer = "pool.ntp.org";

// WiFi icon
const uint8_t wifi_connected_icon[] PROGMEM = {
  0x00, 0x00, 0x07, 0xE0, 0x1F, 0xF8, 0x38, 0x1C,
  0x63, 0xC6, 0x47, 0xE2, 0x0C, 0x30, 0x08, 0x10,
  0x01, 0x80, 0x03, 0xC0, 0x03, 0xC0, 0x01, 0x80,
  0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

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

  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    display.clearDisplay();
    display.println("Storage!");
    display.display();
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
  }
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Initialize Fingerprint
  if (!finger.verifyPassword()) {
    display.clearDisplay();
    display.println("Sensor error!");
    display.display();
  }
  
  loadAndValidateToken();
  display.clearDisplay();
  showCountdown();
}

void loop() {
  maintainWiFi();
  processPendingAttendances();
  sendNewFingerprintsToServer();
  
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