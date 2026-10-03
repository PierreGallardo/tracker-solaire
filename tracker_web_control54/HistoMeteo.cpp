#include "HistoMeteo.h"
#include "Config.h"
#include "CapteursI2C.h"

#include <ArduinoJson.h>
#include <Preferences.h>

static char  histHeure[NB_ECHANT_HISTO_METEO][6];   // "HH:MM"
static float histTemp[NB_ECHANT_HISTO_METEO];
static float histPress[NB_ECHANT_HISTO_METEO];
static int   histIndex  = 0;
static int   histRempli = 0;

static unsigned long lastReleve = 0;

static String tendTemp  = "stable";
static String tendPress = "stable";

// ============================================================
// TENDANCE
// ============================================================

// Moyenne de la premiere moitie du buffer contre celle de la
// seconde. Le seuil evite de reagir au bruit du capteur.
static String calculerTendance(const float* buf, float seuil) {
  if (histRempli < 4) return "stable"; // pas assez d'historique

  int moitie = histRempli / 2;
  float sommeAncien = 0, sommeRecent = 0;
  int nAncien = 0, nRecent = 0;

  for (int i = 0; i < histRempli; i++) {
    int idx = (histIndex - histRempli + i + NB_ECHANT_HISTO_METEO) % NB_ECHANT_HISTO_METEO;
    if (i < moitie) { sommeAncien += buf[idx]; nAncien++; }
    else            { sommeRecent += buf[idx]; nRecent++; }
  }

  float diff = (sommeRecent / nRecent) - (sommeAncien / nAncien);
  if (diff >  seuil) return "up";
  if (diff < -seuil) return "down";
  return "stable";
}

// ============================================================
// RELEVE
// ============================================================

// --- Persistance NVS ----------------------------------------
// Sans elle, l'historique repart de zero a chaque redemarrage :
// avec 30 min entre deux points et 12 h pour remplir le tableau,
// le moindre flashage aneantissait la collecte.
// Format : "HH:MM,temp,press|" par releve, du plus ancien au plus
// recent. 24 points tiennent dans ~500 octets.

static Preferences prefsMeteo;
static bool nvsMeteoOuvert = false;

static void sauvegarderHistoMeteo() {
  if (!nvsMeteoOuvert) return;
  String data = "";
  for (int i = 0; i < histRempli; i++) {
    int idx = (histIndex - histRempli + i + NB_ECHANT_HISTO_METEO) % NB_ECHANT_HISTO_METEO;
    data += String(histHeure[idx]) + "," + String(histTemp[idx], 1)
          + "," + String(histPress[idx], 1) + "|";
  }
  prefsMeteo.putString("histMeteo", data);
}

static void chargerHistoMeteo() {
  String data = prefsMeteo.getString("histMeteo", "");
  if (data.length() == 0) return;

  histIndex  = 0;
  histRempli = 0;

  int pos = 0;
  while (pos < (int)data.length()) {
    int d1 = data.indexOf(',', pos);
    int d2 = data.indexOf(',', d1 + 1);
    int d3 = data.indexOf('|', d2 + 1);
    if (d1 == -1 || d2 == -1 || d3 == -1) break;

    String h = data.substring(pos, d1);
    strncpy(histHeure[histIndex], h.c_str(), sizeof(histHeure[histIndex]) - 1);
    histHeure[histIndex][sizeof(histHeure[histIndex]) - 1] = '\0';
    histTemp[histIndex]  = data.substring(d1 + 1, d2).toFloat();
    histPress[histIndex] = data.substring(d2 + 1, d3).toFloat();

    histIndex = (histIndex + 1) % NB_ECHANT_HISTO_METEO;
    if (histRempli < NB_ECHANT_HISTO_METEO) histRempli++;
    pos = d3 + 1;
  }
  histIndex = histRempli % NB_ECHANT_HISTO_METEO;

  // Les tendances doivent refleter l'historique rechargé, sinon
  // elles repartiraient a "stable" apres chaque redemarrage.
  tendTemp  = calculerTendance(histTemp,  SEUIL_TENDANCE_TEMP_C);
  tendPress = calculerTendance(histPress, SEUIL_TENDANCE_PRESS_HPA);

  Serial.printf("Historique meteo charge : %d releves.\n", histRempli);
}

static void enregistrerReleve() {
  if (!bmp280Present()) return;   // sans capteur, rien a historiser

  // Horodatage court "HH:MM" depuis la RTC.
  char complet[20];
  if (lireHeureRTC(complet, sizeof(complet))) {
    // "AAAA-MM-JJ hh:mm:ss" : l'heure commence a l'offset 11
    strncpy(histHeure[histIndex], complet + 11, 5);
  } else {
    strncpy(histHeure[histIndex], "--:--", 5);
  }
  histHeure[histIndex][5] = '\0';

  histTemp[histIndex]  = lireTemperatureLocaleC();
  histPress[histIndex] = lirePressionLocaleHpa();

  histIndex = (histIndex + 1) % NB_ECHANT_HISTO_METEO;
  if (histRempli < NB_ECHANT_HISTO_METEO) histRempli++;

  sauvegarderHistoMeteo();   // persiste immediatement
  tendTemp  = calculerTendance(histTemp,  SEUIL_TENDANCE_TEMP_C);
  tendPress = calculerTendance(histPress, SEUIL_TENDANCE_PRESS_HPA);
}

void setupHistoMeteo() {
  // Namespace distinct de celui du vent, pour que les deux
  // historiques ne se marchent jamais dessus.
  nvsMeteoOuvert = prefsMeteo.begin("trackmeteo", false);
  if (nvsMeteoOuvert) chargerHistoMeteo();
  else Serial.println("NVS meteo indisponible : historique non conserve.");

  enregistrerReleve();
  lastReleve = millis();
}

void updateHistoMeteo() {
  if (millis() - lastReleve >= PERIODE_HISTO_METEO_MS) {
    lastReleve = millis();
    enregistrerReleve();
  }
}

// ============================================================
// ACCESSEURS
// ============================================================

const String& tendanceTemperature() { return tendTemp; }
const String& tendancePression()    { return tendPress; }

String historiqueMeteoJson() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  // Du plus ancien au plus recent.
  for (int i = 0; i < histRempli; i++) {
    int idx = (histIndex - histRempli + i + NB_ECHANT_HISTO_METEO) % NB_ECHANT_HISTO_METEO;
    JsonObject o = arr.add<JsonObject>();
    o["h"] = histHeure[idx];
    o["t"] = roundf(histTemp[idx]  * 10.0f) / 10.0f;
    o["p"] = roundf(histPress[idx] * 10.0f) / 10.0f;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

String historiqueMeteoCsv() {
  // Virgule decimale : un tableur francais lirait "937.1" comme une date.
  String out = "heure_utc;temperature_C;pression_hPa\r\n";
  for (int i = 0; i < histRempli; i++) {
    int idx = (histIndex - histRempli + i + NB_ECHANT_HISTO_METEO) % NB_ECHANT_HISTO_METEO;
    String t = String(histTemp[idx], 1);
    String p = String(histPress[idx], 1);
    t.replace('.', ',');
    p.replace('.', ',');
    out += String(histHeure[idx]) + ";" + t + ";" + p + "\r\n";
  }
  return out;
}
