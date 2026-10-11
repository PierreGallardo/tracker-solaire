#include "HistoVent.h"
#include "Config.h"
#include "Vent.h"
#include "CapteursI2C.h"

#include <ArduinoJson.h>
#include <Preferences.h>

static char  histHeure[NB_ECHANT_HISTO_VENT][6];   // "HH:MM" (fin de la demi-heure)
static float histMoy[NB_ECHANT_HISTO_VENT];
static float histRaf[NB_ECHANT_HISTO_VENT];
static int   histIndex  = 0;
static int   histRempli = 0;

static String tendVent = "stable";

// Demi-heure en cours
static unsigned long debutFenetre = 0;
static unsigned long dernierEchant = 0;
static float  sommeVent = 0;
static long   nbEchant  = 0;
static float  rafaleMax = 0;

static int indexDe(int i) {
  return (histIndex - histRempli + i + NB_ECHANT_HISTO_VENT) % NB_ECHANT_HISTO_VENT;
}

// ============================================================
// TENDANCE (meme calcul que la temperature et la pression)
// ============================================================

// Moyenne de la premiere moitie du buffer contre celle de la seconde.
static String calculerTendance() {
  if (histRempli < 4) return "stable";

  int moitie = histRempli / 2;
  float sommeAncien = 0, sommeRecent = 0;
  int nAncien = 0, nRecent = 0;
  for (int i = 0; i < histRempli; i++) {
    float v = histMoy[indexDe(i)];
    if (i < moitie) { sommeAncien += v; nAncien++; }
    else            { sommeRecent += v; nRecent++; }
  }

  float diff = (sommeRecent / nRecent) - (sommeAncien / nAncien);
  if (diff >  SEUIL_TENDANCE_HISTO_VENT_KMH) return "up";
  if (diff < -SEUIL_TENDANCE_HISTO_VENT_KMH) return "down";
  return "stable";
}

// ============================================================
// PERSISTANCE NVS (namespace "trackvent", distinct des autres)
// Format : "HH:MM,moy,raf|" par point, du plus ancien au plus recent.
// ============================================================

static Preferences prefsVent30;
static bool nvsOuvert = false;

static void sauvegarder() {
  if (!nvsOuvert) return;
  String data = "";
  for (int i = 0; i < histRempli; i++) {
    int idx = indexDe(i);
    data += String(histHeure[idx]) + "," + String(histMoy[idx], 1)
          + "," + String(histRaf[idx], 1) + "|";
  }
  prefsVent30.putString("hist", data);
}

static void charger() {
  String data = prefsVent30.getString("hist", "");
  if (data.length() == 0) return;

  histIndex = 0;
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
    histMoy[histIndex] = data.substring(d1 + 1, d2).toFloat();
    histRaf[histIndex] = data.substring(d2 + 1, d3).toFloat();

    histIndex = (histIndex + 1) % NB_ECHANT_HISTO_VENT;
    if (histRempli < NB_ECHANT_HISTO_VENT) histRempli++;
    pos = d3 + 1;
  }
  histIndex = histRempli % NB_ECHANT_HISTO_VENT;
  tendVent = calculerTendance();
  Serial.printf("Historique vent (30 min) charge : %d points.\n", histRempli);
}

// ============================================================
// RELEVE
// ============================================================

static void ouvrirFenetre() {
  debutFenetre = dernierEchant = millis();
  sommeVent = 0;
  nbEchant  = 0;
  rafaleMax = 0;
}

static void enregistrerPoint() {
  if (nbEchant == 0) return;   // rien mesure (ne devrait pas arriver)

  char complet[20];
  if (lireHeureRTC(complet, sizeof(complet))) strncpy(histHeure[histIndex], complet + 11, 5);
  else                                       strncpy(histHeure[histIndex], "--:--", 5);
  histHeure[histIndex][5] = '\0';

  histMoy[histIndex] = sommeVent / nbEchant;
  histRaf[histIndex] = rafaleMax;

  histIndex = (histIndex + 1) % NB_ECHANT_HISTO_VENT;
  if (histRempli < NB_ECHANT_HISTO_VENT) histRempli++;

  sauvegarder();
  tendVent = calculerTendance();
}

void setupHistoVent() {
  nvsOuvert = prefsVent30.begin("trackvent", false);
  if (nvsOuvert) charger();
  else Serial.println("NVS vent (30 min) indisponible : historique non conserve.");
  ouvrirFenetre();
}

void updateHistoVent() {
  // La rafale est suivie a chaque tour : un pic de 2 s ne doit pas
  // tomber entre deux echantillons.
  float r = ventRafaleKmh();
  if (r > rafaleMax) rafaleMax = r;

  if (millis() - dernierEchant >= PERIODE_ECHANT_HISTO_VENT_MS) {
    dernierEchant = millis();
    sommeVent += ventVitesseKmh();
    nbEchant++;
  }

  if (millis() - debutFenetre >= PERIODE_HISTO_VENT_MS) {
    enregistrerPoint();
    ouvrirFenetre();
  }
}

// ============================================================
// ACCESSEURS
// ============================================================

const String& tendanceHistoVent() { return tendVent; }

String historiqueVent30Json() {
  JsonDocument doc;
  JsonArray arr = doc["points"].to<JsonArray>();
  for (int i = 0; i < histRempli; i++) {
    int idx = indexDe(i);
    JsonObject o = arr.add<JsonObject>();
    o["h"] = histHeure[idx];
    o["m"] = roundf(histMoy[idx] * 10.0f) / 10.0f;
    o["r"] = roundf(histRaf[idx] * 10.0f) / 10.0f;
  }
  doc["tendance"] = tendVent;

  JsonObject c = doc["enCours"].to<JsonObject>();
  c["min"] = (long)((millis() - debutFenetre) / 60000UL);
  c["m"]   = nbEchant ? roundf(sommeVent / nbEchant * 10.0f) / 10.0f : 0.0f;
  c["r"]   = roundf(rafaleMax * 10.0f) / 10.0f;

  String out;
  serializeJson(doc, out);
  return out;
}

String historiqueVent30Csv() {
  String out = "heure_utc;vent_moyen_kmh;rafale_max_kmh\r\n";
  for (int i = 0; i < histRempli; i++) {
    int idx = indexDe(i);
    String m = String(histMoy[idx], 1);
    String r = String(histRaf[idx], 1);
    m.replace('.', ',');
    r.replace('.', ',');
    out += String(histHeure[idx]) + ";" + m + ";" + r + "\r\n";
  }
  return out;
}
