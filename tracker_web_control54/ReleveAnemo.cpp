#include "ReleveAnemo.h"
#include "Config.h"
#include "Vent.h"
#include "Meteo.h"
#include "CapteursI2C.h"

#include <ArduinoJson.h>
#include <Preferences.h>

struct Releve {
  char  heure[6];   // "HH:MM" UTC, fin de la tranche
  float vMoy;       // tension moyenne (V) pendant la tranche
  float vMax;       // tension maximale (V)
  float regVent;    // vent regional (km/h), -1 si inconnu
  float regRaf;     // rafale regionale (km/h), -1 si inconnue
};

static Releve lignes[NB_RELEVES_ANEMO];
static int    idxEcriture = 0;
static int    nbLignes    = 0;

// Tranche en cours
static unsigned long debutTranche = 0, dernierEchant = 0;
static float  sommeV = 0, maxV = 0;
static long   nbEch = 0, nbActifs = 0;

static int indexDe(int i) {
  return (idxEcriture - nbLignes + i + NB_RELEVES_ANEMO) % NB_RELEVES_ANEMO;
}

// Constante suggeree pour une ligne, ou -1 si la ligne n'est pas
// exploitable (vent trop faible : le rapport serait domine par le bruit).
static float constanteLigne(const Releve &r) {
  if (r.regVent < RELEVE_VENT_REG_MIN_KMH || r.vMoy < RELEVE_TENSION_MIN_V) return -1;
  return r.regVent / r.vMoy;
}

// Mediane des constantes exploitables : un releve aberrant (rafale
// locale isolee, meteo en retard) ne la deplace presque pas.
static float mediane(int &nValides) {
  float v[NB_RELEVES_ANEMO];
  nValides = 0;
  for (int i = 0; i < nbLignes; i++) {
    float k = constanteLigne(lignes[indexDe(i)]);
    if (k > 0) v[nValides++] = k;
  }
  if (nValides == 0) return -1;
  for (int i = 1; i < nValides; i++) {          // tri par insertion
    float x = v[i]; int j = i - 1;
    while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
    v[j + 1] = x;
  }
  return (nValides % 2) ? v[nValides / 2] : (v[nValides / 2 - 1] + v[nValides / 2]) / 2.0f;
}

// ============================================================
// PERSISTANCE NVS (namespace "trackanemo")
// Format : "HH:MM,vMoy,vMax,regVent,regRaf|" par ligne.
// ============================================================

static Preferences prefsAnemo;
static bool nvsOuvert = false;

static void sauvegarder() {
  if (!nvsOuvert) return;
  String data = "";
  for (int i = 0; i < nbLignes; i++) {
    const Releve &r = lignes[indexDe(i)];
    data += String(r.heure) + "," + String(r.vMoy, 3) + "," + String(r.vMax, 3)
          + "," + String(r.regVent, 1) + "," + String(r.regRaf, 1) + "|";
  }
  prefsAnemo.putString("releves", data);
}

static void charger() {
  String data = prefsAnemo.getString("releves", "");
  int pos = 0;
  while (pos < (int)data.length()) {
    int d[5];
    d[0] = data.indexOf(',', pos);
    for (int k = 1; k < 4; k++) d[k] = (d[k - 1] == -1) ? -1 : data.indexOf(',', d[k - 1] + 1);
    d[4] = (d[3] == -1) ? -1 : data.indexOf('|', d[3] + 1);
    if (d[4] == -1) break;

    Releve &r = lignes[idxEcriture];
    String h = data.substring(pos, d[0]);
    strncpy(r.heure, h.c_str(), 5); r.heure[5] = '\0';
    r.vMoy    = data.substring(d[0] + 1, d[1]).toFloat();
    r.vMax    = data.substring(d[1] + 1, d[2]).toFloat();
    r.regVent = data.substring(d[2] + 1, d[3]).toFloat();
    r.regRaf  = data.substring(d[3] + 1, d[4]).toFloat();

    idxEcriture = (idxEcriture + 1) % NB_RELEVES_ANEMO;
    if (nbLignes < NB_RELEVES_ANEMO) nbLignes++;
    pos = d[4] + 1;
  }
  if (nbLignes) Serial.printf("Releves anemometre charges : %d lignes.\n", nbLignes);
}

// ============================================================
// RELEVE
// ============================================================

static void ouvrirTranche() {
  debutTranche = dernierEchant = millis();
  sommeV = maxV = 0;
  nbEch = nbActifs = 0;
}

static void cloreTranche() {
  // Une ligne seulement si l'anemometre a tourne une bonne partie de
  // la tranche : par temps calme, le tableau ne se remplit pas de zeros.
  if (nbEch == 0 || nbActifs * 100 < nbEch * RELEVE_ANEMO_ACTIF_MIN_PCT) return;

  Releve &r = lignes[idxEcriture];
  char complet[20];
  if (lireHeureRTC(complet, sizeof(complet))) strncpy(r.heure, complet + 11, 5);
  else                                       strncpy(r.heure, "--:--", 5);
  r.heure[5] = '\0';
  r.vMoy = sommeV / nbEch;
  r.vMax = maxV;

  bool meteoFraiche = meteo.valide && (millis() - meteo.derniereMaj <= RELEVE_METEO_AGE_MAX_MS);
  r.regVent = meteoFraiche ? meteo.ventKmh   : -1;
  r.regRaf  = meteoFraiche ? meteo.rafaleKmh : -1;

  idxEcriture = (idxEcriture + 1) % NB_RELEVES_ANEMO;
  if (nbLignes < NB_RELEVES_ANEMO) nbLignes++;
  sauvegarder();
}

void setupReleveAnemo() {
  nvsOuvert = prefsAnemo.begin("trackanemo", false);
  if (nvsOuvert) charger();
  else Serial.println("NVS releves anemometre indisponible.");
  ouvrirTranche();
}

void updateReleveAnemo() {
  // Le maximum est suivi a chaque tour (la tension est rafraichie
  // toutes les 100 ms) ; la moyenne, une fois par seconde.
  float v = anemoTensionV();
  if (v > maxV) maxV = v;

  if (millis() - dernierEchant >= PERIODE_ECHANT_RELEVE_MS) {
    dernierEchant = millis();
    sommeV += v;
    nbEch++;
    if (v >= ANEMO_SEUIL_ACTIVITE_V) nbActifs++;
  }

  if (millis() - debutTranche >= PERIODE_RELEVE_ANEMO_MS) {
    cloreTranche();
    ouvrirTranche();
  }
}

// ============================================================
// ACCESSEURS
// ============================================================

static float arrondi(float x, float p) { return roundf(x * p) / p; }

String releveAnemoJson() {
  JsonDocument doc;
  doc["constante"] = FACTEUR_VOLT_KMH;
  int nValides;
  float med = mediane(nValides);
  if (med > 0) doc["suggeree"] = arrondi(med, 10); else doc["suggeree"] = nullptr;
  doc["nValides"] = nValides;

  JsonArray arr = doc["lignes"].to<JsonArray>();
  for (int i = 0; i < nbLignes; i++) {
    const Releve &r = lignes[indexDe(i)];
    JsonObject o = arr.add<JsonObject>();
    o["h"]  = r.heure;
    o["v"]  = arrondi(r.vMoy, 1000);
    o["vx"] = arrondi(r.vMax, 1000);
    o["l"]  = arrondi(r.vMoy * FACTEUR_VOLT_KMH, 10);
    o["lr"] = arrondi(r.vMax * FACTEUR_VOLT_KMH, 10);
    if (r.regVent >= 0) o["r"]  = r.regVent; else o["r"]  = nullptr;
    if (r.regRaf  >= 0) o["rr"] = r.regRaf;  else o["rr"] = nullptr;
    float k = constanteLigne(r);
    if (k > 0) o["k"] = arrondi(k, 10); else o["k"] = nullptr;
  }

  JsonObject c = doc["enCours"].to<JsonObject>();
  c["min"]      = (long)((millis() - debutTranche) / 60000UL);
  c["actifPct"] = nbEch ? (int)(nbActifs * 100 / nbEch) : 0;

  String out;
  serializeJson(doc, out);
  return out;
}

static String virgule(float x, int dec) {
  if (x < 0) return "";
  String s = String(x, dec);
  s.replace('.', ',');
  return s;
}

String releveAnemoCsv() {
  String out = "heure_utc;tension_moy_V;tension_max_V;vent_local_kmh;rafale_locale_kmh;"
               "vent_regional_kmh;rafale_regionale_kmh;constante_suggeree\r\n";
  for (int i = 0; i < nbLignes; i++) {
    const Releve &r = lignes[indexDe(i)];
    out += String(r.heure) + ";" + virgule(r.vMoy, 3) + ";" + virgule(r.vMax, 3) + ";"
         + virgule(r.vMoy * FACTEUR_VOLT_KMH, 1) + ";" + virgule(r.vMax * FACTEUR_VOLT_KMH, 1) + ";"
         + virgule(r.regVent, 1) + ";" + virgule(r.regRaf, 1) + ";"
         + virgule(constanteLigne(r), 1) + "\r\n";
  }
  return out;
}
