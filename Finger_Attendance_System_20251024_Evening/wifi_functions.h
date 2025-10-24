#ifndef WIFI_FUNCTIONS_H
#define WIFI_FUNCTIONS_H

#include "config.h"
#include "globals.h"
#include "sd_functions.h"

// WiFi Functions
void initWiFi(const WiFiConfig &config) {
  if (config.ssid.length() == 0) {
    Serial.println("No WiFi credentials configured");
    return;
  }

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());

  Serial.print("Connecting to WiFi");
  unsigned long startTime = millis();
  const unsigned long timeout = 30000;

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
    delay(500);
    Serial.print(".");
    if (millis() - startTime > 10000 && WiFi.status() == WL_NO_SSID_AVAIL) {
      Serial.println("\nNetwork not found");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nConnected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (ntpServer && ntpServer != "pool.ntp.org") {
      free((void *)ntpServer);
    }
    ntpServer = strdup(config.ntpServer.c_str());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("\nConnection failed!");
  }
}

void maintainWiFi() {
  static unsigned long lastCheck = 0;
  const unsigned long checkInterval = 30000;

  if (millis() - lastCheck > checkInterval) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      WiFi.disconnect();
      delay(100);
      WiFi.reconnect();
      delay(100);
    } else if (!wifiConnected) {
      wifiConnected = true;
      Serial.println("WiFi reconnected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
}

// Web Server Handlers
void handleRoot() {
  String html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>ChekinPlus Configuration</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial; margin: 20px; }
        .container { max-width: 400px; margin: 0 auto; }
        input { width: 100%; padding: 10px; margin: 8px 0; }
        button { background: #0066cc; color: white; padding: 10px; border: none; width: 100%; }
      </style>
    </head>
    <body>
      <div class="container">
        <h2>ChekinPlus Setup</h2>
        <form action="/save" method="post">
          <label for="ssid">WiFi SSID:</label>
          <input type="text" id="ssid" name="ssid" required>
          <label for="password">WiFi Password:</label>
          <input type="password" id="password" name="password">
          <label for="ntp">NTP Server:</label>
          <input type="text" id="ntp" name="ntp" value="pool.ntp.org">
          <button type="submit">Save Settings</button>
        </form>
      </div>
    </body>
    </html>
    )=====";
  server.send(200, "text/html", html);
}

void handleSaveSD() {
  if (server.hasArg("ssid")) {
    WiFiConfig config;
    config.ssid = server.arg("ssid");
    config.password = server.arg("password");
    config.ntpServer = server.hasArg("ntp") ? server.arg("ntp") : "pool.ntp.org";

    if (saveWiFiConfig(config)) {
      String html = R"=====(
        <!DOCTYPE html>
        <html><head>
          <meta http-equiv="refresh" content="5;url=/">
          <title>Settings Saved</title>
        </head>
        <body>
          <h2>Settings Saved to Device!</h2>
          <p>Device will restart in 5 seconds...</p>
        </body></html>
        )=====";
      server.send(200, "text/html", html);
      delay(1000);
      ESP.restart();
    } else {
      server.send(500, "text/plain", "Save to Device Error!");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void enterConfigMode() {
  WiFi.disconnect(true);
  delay(1000);

  String apName = "ChekinPlus-Config";
  WiFi.softAP(apName.c_str());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", handleSaveSD);
  server.onNotFound(handleRoot);
  server.begin();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("CONFIG MODE");
  display.setCursor(0, 16);
  display.print("SSID: ");
  display.println(apName);
  display.setCursor(0, 32);
  display.println("IP: 192.168.4.1");
  display.display();

  unsigned long configStartTime = millis();
  const unsigned long timeout = 60000;

  while (millis() - configStartTime < timeout) {
    dnsServer.processNextRequest();
    server.handleClient();
    delay(10);
    if (server.hasArg("ssid")) break;
  }

  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
}

#endif