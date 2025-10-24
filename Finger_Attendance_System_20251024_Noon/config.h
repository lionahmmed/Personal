#ifndef CONFIG_H
#define CONFIG_H

// Device/Company Info
const String device_id = "D10001";
const String default_token = "SECRETKEY789";
const String cid = "TFL";

// Server Configuration
const String base_url = "http://34.142.226.62:9001";

// File Paths
const String TOKEN_FILE = "/auth_token.txt";
const String PENDING_ATTENDANCE_FILE = "/pending_attendance.csv";
const String PENDING_FINGERPRINTS_FILE = "/pending_fingerprints.csv";

// Timing Constants
const unsigned long SYNC_INTERVAL = 5 * 60 * 1000;
const unsigned long SYNC_RETRY_DELAY = 1 * 60 * 1000;
const int MAX_RETRIES = 3;
const int MAX_FINGERPRINT_RECORDS = 50;

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

// Other Constants
#define LONG_PRESS_THRESHOLD 1000
const unsigned long menuTimeout = 10000;

// WiFi Configuration
const long gmtOffset_sec = 6 * 3600;
const int daylightOffset_sec = 0;

// WiFi Configuration Structure
struct WiFiConfig {
  String ssid;
  String password;
  String ntpServer = "pool.ntp.org";
};

#endif