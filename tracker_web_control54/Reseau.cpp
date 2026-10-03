#include "Reseau.h"
#include "Config.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

bool connectWiFi() {
  Serial.println("\nConnexion au WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi CONNECTÉ ! IP : ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("ÉCHEC WiFi");
  return false;
}

void demarrerMDNS() {
  if (MDNS.begin(MDNS_HOSTNAME)) {
    Serial.println("mDNS actif : accessible aussi via http://" MDNS_HOSTNAME ".local");
  } else {
    Serial.println("mDNS non demarre (utilise l'adresse IP directement).");
  }
}

// ============================================================
// FLASHAGE OVER-THE-AIR
// ============================================================

void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("\n=== OTA : debut du flashage ===");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n=== OTA : flashage termine, redemarrage en cours ===");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total > 0) Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)         Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)     Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA pret (port 3232, hote " OTA_HOSTNAME ")");
}

void gererOTA() {
  ArduinoOTA.handle();
}
