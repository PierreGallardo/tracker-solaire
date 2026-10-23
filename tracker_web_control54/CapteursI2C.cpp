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

// Fuseau Europe/Paris en regle POSIX : CET (UTC+1) l'hiver, CEST
// (UTC+2) du dernier dimanche de mars 2 h au dernier dimanche
// d'octobre 3 h. Le changement d'heure est donc automatique.
static const char TZ_EUROPE_PARIS[] = "CET-1CEST,M3.5.0,M10.5.0/3";

// Au-dela du 1er janvier 2024 : l'horloge systeme a ete reglee par NTP
// (au demarrage elle part de 1970).
static const time_t EPOCH_NTP_MIN = 1704067200;

// Demarre l'heure NTP (appele une fois si le WiFi est up) et regle la RTC
void synchroniserRTCDepuisNTP() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Le SNTP continue ensuite seul en tache de fond (resynchro horaire).
  configTzTime(TZ_EUROPE_PARIS, "pool.ntp.org", "time.google.com");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 8000)) {
    Serial.println("Echec synchro NTP, la RTC garde son heure actuelle.");
    return;
  }
  Serial.println("Heure NTP obtenue (fuseau Europe/Paris).");
  if (!rtcOK) return;

  // La RTC reste en UTC : getLocalTime() rend maintenant l'heure de
  // Paris, on repart donc de l'heure systeme convertie en UTC.
  time_t t = time(nullptr);
  struct tm utc;
  gmtime_r(&t, &utc);
  rtc.adjust(DateTime(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                       utc.tm_hour, utc.tm_min, utc.tm_sec));
  Serial.println("RTC synchronisee via NTP (UTC).");
}

bool heureNTPValide() { return time(nullptr) >= EPOCH_NTP_MIN; }

uint32_t epochNTP() { return heureNTPValide() ? (uint32_t)time(nullptr) : 0; }

long decalageParisSecondes() {
  time_t t = time(nullptr);
  struct tm loc, utc;
  localtime_r(&t, &loc);
  gmtime_r(&t, &utc);
  // Ecart entre les deux decompositions : jamais plus d'un jour.
  long d = (loc.tm_hour - utc.tm_hour) * 3600L + (loc.tm_min - utc.tm_min) * 60L;
  if (loc.tm_yday != utc.tm_yday) d += (loc.tm_year > utc.tm_year || loc.tm_yday > utc.tm_yday) ? 86400L : -86400L;
  return d;
}

bool formaterHeureParis(char* buf, size_t taille, const char* format) {
  if (!heureNTPValide()) return false;
  time_t t = time(nullptr);
  struct tm loc;
  localtime_r(&t, &loc);
  return strftime(buf, taille, format, &loc) > 0;
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

// AJOUT 2 : horodatage des barres du bargraphe des rafales.
bool lireEpochRTC(uint32_t& epoch) {
  if (!rtcOK) return false;
  epoch = rtc.now().unixtime();
  return true;
}
