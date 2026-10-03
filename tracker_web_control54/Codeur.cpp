#include "Codeur.h"
#include "Config.h"

#include <ArduinoJson.h>
#include "driver/gpio.h"
#include "esp_timer.h"

static volatile int32_t  position    = 0;
static volatile uint32_t transitionsA = 0;
static volatile uint32_t transitionsB = 0;
static volatile uint32_t erreurs      = 0;
static volatile uint8_t  etatPrecedent = 0;

static bool          ecouteActive   = false;
static bool          bruitDetecte   = false;
static unsigned long debutEcoute    = 0;
static unsigned long dernierControle = 0;
static uint32_t      transitionsAuDernierControle = 0;

// --- Enregistreur de transitions (diagnostic) ---------------
#define CODEUR_TRACE_MAX 128
static volatile uint32_t traceDt[CODEUR_TRACE_MAX];    // us depuis la transition precedente
static volatile uint8_t  traceEtat[CODEUR_TRACE_MAX];  // etat (A<<1)|B apres la transition
static volatile int      traceN = 0;
static volatile bool     traceArme = false;
static volatile int64_t  traceDernierT = 0;
static uint8_t           traceEtatInitial = 0;

// Table de decodage en quadrature. Etat = (A << 1) | B, index = (ancien << 2) | nouveau.
//  +1 / -1 : un pas dans un sens ou dans l'autre
//   0      : aucun changement
//   2      : transition IMPOSSIBLE (A et B changent ensemble) -> erreur
// Sens "+" : 00 -> 01 -> 11 -> 10 -> 00. Permuter CHA et CHB (ou A et /A) l'inverse.
static const int8_t TABLE[16] = {
   0, +1, -1,  2,    // ancien 00
  -1,  0,  2, +1,    // ancien 01
  +1,  2,  0, -1,    // ancien 10
   2, -1, +1,  0     // ancien 11
};

// Une seule routine pour les deux voies : on relit les deux niveaux
// et on compare a l'etat memorise.
static void IRAM_ATTR isrCodeur() {
  uint8_t a = gpio_get_level((gpio_num_t)PIN_CODEUR_A);
  uint8_t b = gpio_get_level((gpio_num_t)PIN_CODEUR_B);
  uint8_t nouveau  = (a << 1) | b;
  uint8_t ancien   = etatPrecedent;
  if (nouveau == ancien) return;

  // Ecriture explicite : les operateurs composes sur volatile sont
  // deprecies en C++20.
  if ((nouveau ^ ancien) & 2) transitionsA = transitionsA + 1;
  if ((nouveau ^ ancien) & 1) transitionsB = transitionsB + 1;

  int8_t d = TABLE[(ancien << 2) | nouveau];
  if (d == 2) erreurs = erreurs + 1;
  else        position = position + d;

  // Enregistreur de diagnostic : etat apres chaque transition + delai en
  // microsecondes depuis la precedente. Montre la VRAIE sequence des
  // signaux (ordre A/B, ecart entre fronts) quand le comptage semble faux.
  if (traceArme) {
    int n = traceN;
    if (n < CODEUR_TRACE_MAX) {
      int64_t t = esp_timer_get_time();
      traceDt[n]   = (n == 0) ? 0 : (uint32_t)(t - traceDernierT);
      traceEtat[n] = nouveau;
      traceDernierT = t;
      traceN = n + 1;
    } else {
      traceArme = false;
    }
  }

  etatPrecedent = nouveau;
}

// Les deux broches sont des entrees seules, sans pull-up interne : le
// niveau est impose par le MAX490 (via le pont diviseur).
static void configurerBroches() {
  pinMode(PIN_CODEUR_A, INPUT);
  pinMode(PIN_CODEUR_B, INPUT);
}

void codeurRemiseAZero() {
  noInterrupts();
  position = 0; transitionsA = 0; transitionsB = 0; erreurs = 0;
  interrupts();
  transitionsAuDernierControle = 0;
}

void codeurActiver(bool actif) {
  if (actif == ecouteActive) return;

  if (actif) {
    configurerBroches();
    etatPrecedent = (gpio_get_level((gpio_num_t)PIN_CODEUR_A) << 1)
                  |  gpio_get_level((gpio_num_t)PIN_CODEUR_B);
    codeurRemiseAZero();
    bruitDetecte = false;
    debutEcoute = dernierControle = millis();
    attachInterrupt(digitalPinToInterrupt(PIN_CODEUR_A), isrCodeur, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_CODEUR_B), isrCodeur, CHANGE);
    ecouteActive = true;
    Serial.println("Codeur : ecoute ACTIVEE (outil de test).");
  } else {
    detachInterrupt(digitalPinToInterrupt(PIN_CODEUR_A));
    detachInterrupt(digitalPinToInterrupt(PIN_CODEUR_B));
    ecouteActive = false;
    Serial.println("Codeur : ecoute coupee.");
  }
}

void updateCodeur() {
  if (!ecouteActive) return;

  // Arret automatique : l'outil de test ne doit pas rester arme.
  if (millis() - debutEcoute >= CODEUR_DIAG_DUREE_MS) {
    codeurActiver(false);
    return;
  }

  // Controle du taux de transitions, une fois par seconde.
  if (millis() - dernierControle >= 1000) {
    uint32_t total = transitionsA + transitionsB;
    uint32_t delta = total - transitionsAuDernierControle;
    transitionsAuDernierControle = total;
    dernierControle = millis();

    if (delta > CODEUR_TAUX_MAX_PAR_S) {
      bruitDetecte = true;
      codeurActiver(false);
      Serial.printf("Codeur : %lu transitions en 1 s -> bruit, ecoute coupee (entree non reliee ?).\n",
                    (unsigned long)delta);
    }
  }
}

String codeurJson() {
  JsonDocument doc;
  doc["actif"]     = ecouteActive;
  doc["position"]  = (int32_t)position;
  doc["transA"]    = (uint32_t)transitionsA;
  doc["transB"]    = (uint32_t)transitionsB;
  doc["erreurs"]   = (uint32_t)erreurs;
  configurerBroches();
  // Niveaux lus a l'instant, meme ecoute coupee : utile pour verifier
  // d'abord que chaque voie bouge avant d'armer le comptage.
  doc["niveauA"]   = digitalRead(PIN_CODEUR_A);
  doc["niveauB"]   = digitalRead(PIN_CODEUR_B);
  doc["bruit"]     = bruitDetecte;
  long reste = ecouteActive ? (long)((CODEUR_DIAG_DUREE_MS - (millis() - debutEcoute)) / 1000UL) : 0;
  doc["secRestantes"] = reste;

  String out;
  serializeJson(doc, out);
  return out;
}

// ============================================================
// ENREGISTREUR DE TRANSITIONS
// ============================================================

void codeurTraceArmer() {
  // L'enregistreur ne voit que ce que l'ecoute lui transmet.
  if (!ecouteActive) codeurActiver(true);
  noInterrupts();
  traceN = 0;
  traceEtatInitial = etatPrecedent;
  traceArme = true;
  interrupts();
}

String codeurTraceJson() {
  JsonDocument doc;
  doc["armee"] = (bool)traceArme;
  int n = traceN;
  doc["n"] = n;
  char s[3];
  s[0] = '0' + ((traceEtatInitial >> 1) & 1); s[1] = '0' + (traceEtatInitial & 1); s[2] = 0;
  doc["initial"] = s;

  JsonArray arr = doc["evenements"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    uint8_t e = traceEtat[i];
    char t[3] = { (char)('0' + ((e >> 1) & 1)), (char)('0' + (e & 1)), 0 };
    JsonObject o = arr.add<JsonObject>();
    o["dt"] = (uint32_t)traceDt[i];
    o["s"]  = t;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// ============================================================
// COMPTAGE DE SERVICE - COMPTEUR MATERIEL PCNT
// ============================================================

#include "driver/pulse_cnt.h"

static pcnt_unit_handle_t unitePcnt = nullptr;
static bool pcntOK = false;

void setupCodeurService() {
  // Le compteur materiel est limite a 16 bits : avec accum_count, le
  // pilote cumule chaque debordement aux limites pour donner une
  // position sur 32 bits.
  pcnt_unit_config_t cfgUnite = {};
  cfgUnite.low_limit  = -30000;
  cfgUnite.high_limit =  30000;
  cfgUnite.flags.accum_count = 1;
  if (pcnt_new_unit(&cfgUnite, &unitePcnt) != ESP_OK) {
    Serial.println("Codeur : compteur materiel indisponible.");
    return;
  }

  // Filtre : ignore les impulsions de moins de 1 us (parasites).
  pcnt_glitch_filter_config_t filtre = {};
  filtre.max_glitch_ns = 1000;
  pcnt_unit_set_glitch_filter(unitePcnt, &filtre);

  // Quadrature x4 : chaque voie compte ses deux fronts, le sens est
  // donne par le niveau de l'autre voie.
  pcnt_chan_config_t cfgA = {};
  cfgA.edge_gpio_num  = PIN_CODEUR_A;
  cfgA.level_gpio_num = PIN_CODEUR_B;
  pcnt_channel_handle_t voieA = nullptr;
  pcnt_new_channel(unitePcnt, &cfgA, &voieA);

  pcnt_chan_config_t cfgB = {};
  cfgB.edge_gpio_num  = PIN_CODEUR_B;
  cfgB.level_gpio_num = PIN_CODEUR_A;
  pcnt_channel_handle_t voieB = nullptr;
  pcnt_new_channel(unitePcnt, &cfgB, &voieB);

  pcnt_channel_set_edge_action(voieA, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_channel_set_level_action(voieA, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  pcnt_channel_set_edge_action(voieB, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
  pcnt_channel_set_level_action(voieB, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

  // Points de veille aux limites : indispensables au cumul.
  pcnt_unit_add_watch_point(unitePcnt, cfgUnite.low_limit);
  pcnt_unit_add_watch_point(unitePcnt, cfgUnite.high_limit);

  pcnt_unit_enable(unitePcnt);
  pcnt_unit_clear_count(unitePcnt);
  pcnt_unit_start(unitePcnt);
  pcntOK = true;
  Serial.println("Codeur : comptage materiel actif.");
}

bool codeurServiceOK() { return pcntOK; }

int32_t codeurServicePosition() {
  if (!pcntOK) return 0;
  int v = 0;
  pcnt_unit_get_count(unitePcnt, &v);
  return (int32_t)v;
}
