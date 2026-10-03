#include "CapteursI2C.h"
#include "Config.h"

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <RTClib.h>
#include <time.h>

static Adafruit_BMP280 bmp;
static RTC_DS3231      rtc;

static bool bmeOK = false;
static bool rtcOK = false;

bool setupCapteursI2C() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_CLOCK_HZ); // vitesse reduite, plus tolerante au bruit electrique
  delay(100); // laisse le bus/les capteurs se stabiliser

  for (int essai = 0; essai < 3 && !bmeOK; essai++) {
    bmeOK = bmp.begin(0x76, BMP280_CHIP_ID); // clone BMP280 HW-611
    if (!bmeOK) bmeOK = bmp.begin(0x77, BMP280_CHIP_ID);
    if (!bmeOK) delay(100);
  }
  if (bmeOK) {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  }
  Wire.begin(PIN_SDA, PIN_SCL); // re-force les broches
  if (bmeOK) Serial.println("BMP280 detecte.");
  else Serial.println("BMP280 NON detecte (verifie le cablage/adresse).");

  rtcOK = rtc.begin(&Wire);
  Wire.begin(PIN_SDA, PIN_SCL); // re-force les broches (rtc.begin peut les reinitialiser)
  if (rtcOK) Serial.println("RTC DS3231 detectee.");
  else Serial.println("RTC NON detectee (verifie le cablage).");

  return bmeOK || rtcOK;
}

// Regle la RTC depuis un serveur NTP (appele une fois si le WiFi est up)
void synchroniserRTCDepuisNTP() {
  if (!rtcOK || WiFi.status() != WL_CONNECTED) return;

  configTime(0, 0, "pool.ntp.org", "time.google.com");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 8000)) {
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    Serial.println("RTC synchronisee via NTP (UTC).");
  } else {
    Serial.println("Echec synchro NTP, la RTC garde son heure actuelle.");
  }
}

bool bmp280Present() { return bmeOK; }
bool rtcPresente()   { return rtcOK; }

float lirePressionLocaleHpa()  { return bmp.readPressure() / 100.0; }
float lireTemperatureLocaleC() { return bmp.readTemperature(); }

bool lireHeureRTC(char* buf, size_t taille) {
  if (!rtcOK) return false;
  DateTime now = rtc.now();
  snprintf(buf, taille, "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  return true;
}

bool lireDateHeureRTC(int& annee, int& mois, int& jour,
                      int& heure, int& minute, int& seconde) {
  if (!rtcOK) return false;
  DateTime now = rtc.now();
  annee = now.year();  mois   = now.month();  jour    = now.day();
  heure = now.hour();  minute = now.minute(); seconde = now.second();
  return true;
}
