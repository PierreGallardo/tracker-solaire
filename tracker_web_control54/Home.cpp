#include "Home.h"
#include "Config.h"
#include "Codeur.h"

#include <ArduinoJson.h>

static uint32_t detections = 0;
static bool     etatStable = false;       // true = metal devant le capteur
static bool     etatBrut   = false;
static unsigned long changementBrutMs = 0;

// Position du codeur au premier front brut, retenue si la detection est
// confirmee : l anti-rebond retarde la confirmation de 20 ms, la
// position, elle, est celle de l instant ou le metal est arrive.
static int32_t posAuFrontBrut = 0;
static int32_t posDetection   = 0;
static bool    detectionNonLue = false;

// Lecture dans la boucle (pas d'interruption) : le capteur bouge
// lentement (azimut a quelques degres par seconde), et la mesure de
// tension de homeJson() reconfigure la broche, ce qui declenchait de
// fausses interruptions.
#define HOME_ANTI_REBOND_MS 20

static void configurerBroche() {
  // Pull-up interne en plus de la 10 kOhm externe : la broche reste
  // a l'etat haut meme si la resistance externe manque.
  pinMode(PIN_HOME, INPUT_PULLUP);
}

static bool lireBrut() {
  return digitalRead(PIN_HOME) == HOME_ACTIF_NIVEAU;
}

void setupHome() {
  configurerBroche();
  etatStable = etatBrut = lireBrut();
  changementBrutMs = millis();
}

void updateHome() {
  bool b = lireBrut();
  if (b != etatBrut) {
    etatBrut = b;
    changementBrutMs = millis();
    if (b) posAuFrontBrut = codeurServicePosition();
  } else if (b != etatStable && millis() - changementBrutMs >= HOME_ANTI_REBOND_MS) {
    etatStable = b;
    if (etatStable) {
      detections++;   // une arrivee du metal = une detection
      posDetection = posAuFrontBrut;
      detectionNonLue = true;
    }
  }
}

bool homeDetecte() {
  return etatStable;
}

void homeRemiseAZero() {
  detections = 0;
}

String homeJson() {
  JsonDocument doc;
  doc["detecte"]    = etatStable;
  doc["niveau"]     = digitalRead(PIN_HOME);
  doc["detections"] = detections;

  // Tension reelle sur la broche : montre la marge de l'etat bas
  // (doit rester sous ~0,8 V quand le capteur detecte). La lecture
  // analogique reconfigure la broche, on la remet en entree ensuite.
  analogSetPinAttenuation(PIN_HOME, ADC_11db);
  doc["mV"] = analogReadMilliVolts(PIN_HOME);
  configurerBroche();

  String out;
  serializeJson(doc, out);
  return out;
}

bool homeLireDetection(int32_t &pos) {
  if (!detectionNonLue) return false;
  detectionNonLue = false;
  pos = posDetection;
  return true;
}

void homeOublierDetection() {
  detectionNonLue = false;
}
