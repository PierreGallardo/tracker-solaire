#include "ServeurWeb.h"
#include "Config.h"
#include "PageWeb.h"
#include "Controle.h"
#include "Azimut.h"
#include "Inclinaison.h"
#include "Vent.h"
#include "Solaire.h"
#include "Meteo.h"
#include "CapteursI2C.h"
#include "HistoMeteo.h"
#include "HistoVent.h"
#include "ReleveAnemo.h"
#include "EnregRafales.h"
#include "Codeur.h"
#include "PageCodeur.h"
#include "Home.h"
#include "CalibAzimut.h"
#include "PageCalib.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(80);

// Toutes les commandes renvoient l'utilisateur sur la page principale.
static void redirigerVersAccueil() {
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================================================
// PAGE PRINCIPALE
// ============================================================

static void handleRoot() {
  server.send_P(200, "text/html; charset=UTF-8", PAGE_INDEX_HTML);
}

// ============================================================
// STATUT JSON - source unique de l'interface web
// ============================================================

static void handleStatus() {
  double inclinaisonReelle = lireInclinaisonReelle();

  JsonDocument doc;
  doc["az"]     = azimutNormaliseDeg();
  doc["azMesure"] = azimutMesure();
  doc["incl"]   = inclinaisonReelle;
  doc["etatV"]  = etatVertical;
  doc["etatH"]  = etatHorizontal;
  doc["secV"]   = secondesRestantesVertical();
  doc["secH"]   = secondesRestantesHorizontal();
  doc["uptime"] = millis() / 1000;
  doc["rssi"]   = WiFi.RSSI();

  doc["meteoValide"] = meteo.valide;
  doc["temp"]        = meteo.temperatureC;
  doc["vent"]        = meteo.ventKmh;
  doc["pression"]    = meteo.pressionHpa;
  doc["pluie"]       = meteo.probaPluiePct;
  doc["meteoAge"]    = meteo.valide ? (millis() - meteo.derniereMaj) / 1000 : 0;

  doc["bmeOK"] = bmp280Present();
  if (bmp280Present()) {
    doc["pressionLocale"]    = lirePressionLocaleHpa();
    doc["temperatureLocale"] = lireTemperatureLocaleC();
    doc["humiditeLocale"]    = -1; // BMP280 ne mesure pas l'humidite
    doc["trendTemp"]         = tendanceTemperature();
    doc["trendPress"]        = tendancePression();
  }

  // --- Suivi solaire ---
  double azSol, elevSol;
  bool solaireOK = positionSolaire(azSol, elevSol);
  doc["solaireOK"] = solaireOK;
  if (solaireOK) {
    doc["azSolaire"]   = azSol;
    doc["elevSolaire"] = elevSol;
  }
  doc["suiviAuto"]  = suiviAutoActif();
  doc["cibleAuto"]  = cibleSuiviAutoDeg();

  doc["rtcOK"]         = rtcPresente();
  doc["ventKmh"]       = ventVitesseKmh();
  doc["ventRafaleKmh"] = ventRafaleKmh();
  doc["securiteVent"]  = ventSecuriteActive();
  doc["modeVentReduit"] = ventModeReduit();
  doc["secAvantLevee"]  = ventSecondesAvantLevee();
  // AJOUT 1 : mise a plat sur rafales repetees
  doc["rafalesFenetre"]        = ventNbRafalesFenetre();
  doc["miseAPlatRafales"]      = ventMiseAPlatRafales();
  doc["secAvantRedeploiement"] = ventSecondesAvantRedeploiement();
  // AJOUT 2 : etat du bargraphe (le detail est sur /api/rafales)
  doc["enregRafales"]          = enregRafalesActif();
  doc["alerteVent"]     = ventAlerteEnAttente();
  if (ventAlerteEnAttente()) {
    doc["alerteVitesse"] = ventAlerteVitesse();
    doc["alerteCause"]   = ventAlerteCause();
    doc["alerteHeure"]   = ventAlerteHeure();
  }
  doc["anemoSuspect"]   = anemoEstSuspect();
  doc["anemoAcquitte"]  = anemoEstAcquitte();
  doc["anemoCause"]     = anemoCauseSuspicion();
  doc["anemoTensionV"]  = anemoTensionV();
  doc["anemoTensionMax"]= anemoTensionMaxV();
  doc["anemoSilence"]   = anemoSecondesDepuisActivite();
  doc["trend"]         = ventTendance();

  char buf[20];
  if (lireHeureRTC(buf, sizeof(buf))) {
    doc["heureRTC"] = buf;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ============================================================
// HISTORIQUE DES MISES EN SECURITE VENT
// ============================================================

// ============================================================
// DIAGNOSTIC : sur quelle broche l'anemometre est-il cable ?
// ------------------------------------------------------------
// Lit le niveau de toutes les broches LIBRES, en pull-up. Une
// broche en l'air doit lire 1. Celle qui vaut 0, ou qui bascule
// quand on tourne les coupelles, porte le capteur.
//
// Les broches deja affectees (moteurs 25/26/27/14, I2C 18/19,
// potentiometre 36/39) sont volontairement exclues : y toucher
// casserait le PWM ou le bus.
// ============================================================

// Tension sur toutes les entrees ADC1. Permet de voir d'un coup
// d'oeil ou se trouve reellement le signal de l'anemometre, et
// si une broche est imposee par autre chose.
static void handleDiagAnalog() {
  static const int adc1[] = {32, 33, 34, 35, 36, 39};
  const int n = sizeof(adc1) / sizeof(adc1[0]);

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    int p = adc1[i];
    analogSetPinAttenuation(p, ADC_11db);
    // Trois lectures : l'ecart revele une entree qui vit.
    uint32_t v1 = analogReadMilliVolts(p);
    delay(2);
    uint32_t v2 = analogReadMilliVolts(p);
    delay(2);
    uint32_t v3 = analogReadMilliVolts(p);
    JsonObject o = arr.add<JsonObject>();
    o["gpio"] = p;
    o["mV"]   = (v1 + v2 + v3) / 3;
    o["brut"] = analogRead(p);
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleDiagBroches() {
  static const int candidates[] = {4, 5, 13, 15, 16, 17, 21, 22, 23};
  const int n = sizeof(candidates) / sizeof(candidates[0]);

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    int p = candidates[i];
    pinMode(p, INPUT_PULLUP);
    delayMicroseconds(200);          // laisse le pull-up etablir le niveau
    JsonObject o = arr.add<JsonObject>();
    o["gpio"]   = p;
    o["niveau"] = digitalRead(p);
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// --- Outil de test du codeur Baumer -------------------------
static void handlePageCodeur()   { server.send_P(200, "text/html; charset=UTF-8", PAGE_CODEUR_HTML); }
static void handleCodeurJson()   { server.send(200, "application/json", codeurJson()); }
static void handleCodeurActif()  { codeurActiver(server.arg("on") == "1"); server.send(200, "text/plain", "ok"); }
static void handleCodeurTrace()  { if (server.arg("go") == "1") { codeurTraceArmer(); server.send(200, "text/plain", "ok"); } else { server.send(200, "application/json", codeurTraceJson()); } }
static void handleCodeurZero()   { codeurRemiseAZero(); server.send(200, "text/plain", "ok"); }
static void handleHome()         { if (server.arg("zero") == "1") homeRemiseAZero(); server.send(200, "application/json", homeJson()); }

static void handleHistoriqueMeteoCsv() {
  server.sendHeader("Content-Disposition", "attachment; filename=\"releves_meteo.csv\"");
  server.send(200, "text/csv; charset=UTF-8", historiqueMeteoCsv());
}

// ============================================================
// AJOUT 2 - BARGRAPHE DES RAFALES
// ------------------------------------------------------------
// Jusqu a 5760 barres : la reponse (environ 25 Ko au pire) est
// envoyee par morceaux, sans jamais la construire en entier en RAM.
// /api/rafales?depuis=<heure UTC> ne renvoie que les barres plus
// recentes : le navigateur ne recharge pas tout toutes les 15 s.
// ============================================================

static void ecrireMorceauHttp(const char* texte) { server.sendContent(texte); }

static void handleRafales() {
  uint32_t depuis = (uint32_t)strtoul(server.arg("depuis").c_str(), nullptr, 10);
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  enregRafalesJson(depuis, ecrireMorceauHttp);
  server.sendContent("");   // fin de la reponse par morceaux
}

#if SIMULATION_VENT
// TESTS : /api/sim-vent?kmh=35  ou  /api/sim-vent?scenario=rafales
static void handleSimVent() {
  if (server.hasArg("scenario")) simulationVentScenario();
  else simulationVentFixer(server.arg("kmh").toFloat());
  server.send(200, "text/plain; charset=UTF-8", "Simulation du vent prise en compte.");
}
#endif

static void handleRelevesAnemo() {
  server.send(200, "application/json", releveAnemoJson());
}

static void handleRelevesAnemoCsv() {
  server.sendHeader("Content-Disposition", "attachment; filename=\"releves_anemometre.csv\"");
  server.send(200, "text/csv; charset=UTF-8", releveAnemoCsv());
}

static void handleRelevesVent() {
  server.send(200, "application/json", historiqueVent30Json());
}

static void handleRelevesVentCsv() {
  server.sendHeader("Content-Disposition", "attachment; filename=\"releves_vent.csv\"");
  server.send(200, "text/csv; charset=UTF-8", historiqueVent30Csv());
}

static void handleHistoriqueMeteo() {
  server.send(200, "application/json", historiqueMeteoJson());
}

static void handleEpisodeVent() {
  server.send(200, "application/json", episodeVentJson());
}

static void handleHistoriqueVent() {
  server.send(200, "application/json", historiqueVentJson());
}

// ============================================================
// CALIBRATION DE L AZIMUT
// ------------------------------------------------------------
// Les commandes de calibration font tourner le tracker : elles ne
// sont acceptees qu en local. Une requete passee par le reverse
// proxy porte l en-tete X-Forwarded-For : elle est refusee.
// ============================================================

static bool refuserSiDistant() {
  if (!server.hasHeader("X-Forwarded-For")) return false;
  server.send(403, "text/plain; charset=UTF-8", "Calibration refusee a distance : utilise le reseau local.");
  return true;
}

static void repondreCalib(const String &msg) {
  server.send(200, "text/plain; charset=UTF-8", msg);
}

static void handlePageCalib()    { server.send_P(200, "text/html; charset=UTF-8", PAGE_CALIB_HTML); }
static void handleCalibJson()    { server.send(200, "application/json", calibJson()); }
static void handleCalibJog() {
  if (refuserSiDistant()) return;
  String d = server.arg("d");
  if (d == "g")      commandeGauche();
  else if (d == "d") commandeDroite();
  else               commandeStopHorizontal();
  repondreCalib("Commande : " + etatHorizontal);
}
static void handleCalibStop()    { if (refuserSiDistant()) return; repondreCalib(calibArreterRecherche()); }
static void handleCalibSens()    { if (refuserSiDistant()) return; repondreCalib(calibChoisirSensEst(server.arg("est") == "g" ? -1 : server.arg("est") == "d" ? 1 : 0)); }
static void handleCalibPoint() {
  if (refuserSiDistant()) return;
  String v = server.arg("az");
  v.replace(",", ".");
  double az = v.length() ? v.toDouble() : NAN;
  repondreCalib(calibReleverPoint(server.arg("n").toInt(), az));
}
static void handleCalibHome()    { if (refuserSiDistant()) return; repondreCalib(calibLancerRechercheHome()); }
static void handleCalibButee()   { if (refuserSiDistant()) return; repondreCalib(calibButee(server.arg("cote") == "est")); }
static void handleCalibValider() { if (refuserSiDistant()) return; repondreCalib(calibValider()); }
static void handleCalibEffacer() { if (refuserSiDistant()) return; repondreCalib(calibEffacer()); }

// ============================================================
// HANDLERS DE COMMANDE
// ============================================================

static void handleMonter()         { commandeMonter();         redirigerVersAccueil(); }
static void handleStopVertical()   { commandeStopVertical();   redirigerVersAccueil(); }
static void handleDescendre()      { commandeDescendre();      redirigerVersAccueil(); }
static void handleRetourAPlat()    { commandeRetourAPlat();    redirigerVersAccueil(); }

static void handleAcquitterAnemo() { acquitterAnemo();        redirigerVersAccueil(); }
static void handleAcquitterVent()  { acquitterVent();         redirigerVersAccueil(); }

static void handleSuiviAuto()      { activerSuiviAuto();      redirigerVersAccueil(); }
static void handleModeManuel()     { desactiverSuiviAuto();   redirigerVersAccueil(); }

static void handleGauche()         { commandeGauche();         redirigerVersAccueil(); }
static void handleStopHorizontal() { commandeStopHorizontal(); redirigerVersAccueil(); }
static void handleDroite()         { commandeDroite();         redirigerVersAccueil(); }

// ============================================================
// DEMARRAGE
// ============================================================

void setupServeurWeb() {
  // En-tete pose par le reverse proxy : sert a refuser la calibration a distance.
  static const char *entetes[] = { "X-Forwarded-For" };
  server.collectHeaders(entetes, 1);

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/api/historique-vent", handleHistoriqueVent);
  server.on("/api/episode-vent", handleEpisodeVent);
  server.on("/api/historique-meteo", handleHistoriqueMeteo);
  server.on("/api/historique-meteo.csv", handleHistoriqueMeteoCsv);
  server.on("/api/releves-vent", handleRelevesVent);
  server.on("/api/releves-vent.csv", handleRelevesVentCsv);
  server.on("/api/rafales", handleRafales);              // AJOUT 2
#if SIMULATION_VENT
  server.on("/api/sim-vent", handleSimVent);             // TESTS uniquement
#endif
  server.on("/api/releves-anemo", handleRelevesAnemo);
  server.on("/api/releves-anemo.csv", handleRelevesAnemoCsv);
  server.on("/codeur", handlePageCodeur);
  server.on("/api/codeur", handleCodeurJson);
  server.on("/api/codeur-actif", handleCodeurActif);
  server.on("/api/codeur-zero", handleCodeurZero);
  server.on("/api/home", handleHome);
  server.on("/calibration", handlePageCalib);
  server.on("/api/calib", handleCalibJson);
  server.on("/api/calib-jog", handleCalibJog);
  server.on("/api/calib-stop", handleCalibStop);
  server.on("/api/calib-sens", handleCalibSens);
  server.on("/api/calib-point", handleCalibPoint);
  server.on("/api/calib-home", handleCalibHome);
  server.on("/api/calib-butee", handleCalibButee);
  server.on("/api/calib-valider", handleCalibValider);
  server.on("/api/calib-effacer", handleCalibEffacer);
  server.on("/api/codeur-trace", handleCodeurTrace);
  server.on("/api/diag-broches", handleDiagBroches);
  server.on("/api/diag-analog", handleDiagAnalog);
  server.on("/monter", handleMonter);
  server.on("/stop_vertical", handleStopVertical);
  server.on("/descendre", handleDescendre);
  server.on("/retour_a_plat", handleRetourAPlat);
  server.on("/suivi_auto", handleSuiviAuto);
  server.on("/mode_manuel", handleModeManuel);
  server.on("/acquitter_anemo", handleAcquitterAnemo);
  server.on("/acquitter_vent", handleAcquitterVent);
  server.on("/gauche", handleGauche);
  server.on("/stop_horizontal", handleStopHorizontal);
  server.on("/droite", handleDroite);
  server.begin();
}

void serveurWebLoop() {
  server.handleClient();
}
