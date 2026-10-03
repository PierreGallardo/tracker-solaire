#include "Meteo.h"
#include "Config.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

MeteoEnLigne meteo;

void fetchMeteo() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE_DEG, 4) +
               "&longitude=" + String(LONGITUDE_DEG, 4) +
               "&current=temperature_2m,wind_speed_10m,surface_pressure" +
               "&hourly=precipitation_probability&forecast_hours=3&timezone=auto";

  http.begin(url);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      meteo.temperatureC  = doc["current"]["temperature_2m"] | 0.0;
      meteo.ventKmh       = doc["current"]["wind_speed_10m"] | 0.0;
      meteo.pressionHpa   = doc["current"]["surface_pressure"] | 0.0;
      meteo.probaPluiePct = doc["hourly"]["precipitation_probability"][1] | 0;
      meteo.valide = true;
      meteo.derniereMaj = millis();
      Serial.println("Meteo mise a jour.");
    } else {
      Serial.println("Erreur parsing JSON meteo.");
    }
  } else {
    Serial.printf("Erreur requete meteo, code HTTP: %d\n", code);
  }
  http.end();
}

void updateMeteo() {
  if (millis() - meteo.derniereMaj >= METEO_INTERVAL_MS) {
    fetchMeteo();
  }
}
