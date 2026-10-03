#include "CalibAzimut.h"
#include "Config.h"
#include "Codeur.h"
#include "Home.h"
#include "Moteurs.h"
#include "Azimut.h"
#include "Controle.h"

#include <Preferences.h>
#include <ArduinoJson.h>
#include <math.h>

// ============================================================
// CALIBRATION ENREGISTREE (NVS, namespace "trackazim")
// ============================================================

static Preferences prefsAz;

static bool   enrValide   = false;
static double enrK        = 0;      // pas codeur par degre d'azimut (signe)
static double enrAzHome   = 0;      // azimut du capteur HOME
static double enrButEst   = 0;      // butees logicielles (degres)
static double enrButOuest = 0;
static int8_t enrSensEst  = 0;      // commande moteur qui tourne vers l'EST

// ============================================================
// SESSION DE CALIBRATION EN COURS (non enregistree)
// ============================================================

static int8_t  sessSensEst = 0;
static bool    p1Ok = false, p2Ok = false;
static int32_t p1 = 0, p2 = 0;
static double  az1 = 0, az2 = 0;
static bool    sessKOk = false;
static double  sessK = 0;
static bool    ctrlOk = false;
static double  ctrlBoussole = 0, ctrlCalcule = 0;
static bool    sessHomeOk = false;
static double  sessAzHome = 0;
static bool    butEstOk = false, butOuestOk = false;
static double  butEst = 0, butOuest = 0;

// ============================================================
// REFERENCE ACTIVE : azimut = azRef + (pos - posRef) / kRef
// ============================================================

enum SourceRef { REF_AUCUNE, REF_POINTS, REF_HOME };
static SourceRef sourceRef = REF_AUCUNE;
static int32_t   posRef = 0;
static double    azRef  = 0;
static double    kRef   = 0;

static String message = "";

static bool azPourPosition(int32_t pos, double &az) {
  if (sourceRef == REF_AUCUNE || kRef == 0) return false;
  az = azRef + (double)(pos - posRef) / kRef;
  return true;
}

static int8_t sensEstActif() {
  return sessSensEst != 0 ? sessSensEst : enrSensEst;
}

// ============================================================
// RECHERCHE DU HOME
// ------------------------------------------------------------
// Le HOME est toujours approche dans le meme sens (vers l'EST) pour
// que la position retenue soit repetable malgre le jeu mecanique.
// S'il est deja devant le capteur, le tracker recule d'abord vers
// l'OUEST pour le quitter.
// ============================================================

enum Recherche { R_AUCUNE, R_DEGAGEMENT, R_DEGAGEMENT_FIN, R_PAUSE, R_APPROCHE };
static Recherche     rech = R_AUCUNE;
static unsigned long rechDebut = 0, phaseDebut = 0;
static unsigned long blocageDebut = 0;
static int32_t       blocagePos = 0;
static bool          sensVerifie = false;
static double        azDebutApproche = 0;
static bool          azDebutConnu = false;

static const char* nomRecherche() {
  switch (rech) {
    case R_DEGAGEMENT:     return "degagement";
    case R_DEGAGEMENT_FIN: return "degagement";
    case R_PAUSE:          return "pause";
    case R_APPROCHE:       return "approche";
    default:               return "aucune";
  }
}

static void moteur(int8_t sens) {
  driveAzimut(sens == 0 ? 1 : sens, sens == 0 ? 0 : CALIB_VITESSE_HOME);
  setSensAzimut(sens);
}

static void surveillerBlocage(bool reinitialiser) {
  int32_t pos = codeurServicePosition();
  if (reinitialiser || abs(pos - blocagePos) >= CALIB_BLOCAGE_PAS_MIN) {
    blocagePos = pos;
    blocageDebut = millis();
  }
}

static void arreterRecherche(const String &msg) {
  moteur(0);
  etatHorizontal = "STOP";
  rech = R_AUCUNE;
  message = msg;
  Serial.println("Calibration : " + msg);
}

static void entrerPhase(Recherche r) {
  rech = r;
  phaseDebut = millis();
  surveillerBlocage(true);
  int8_t est = sensEstActif();
  if (r == R_DEGAGEMENT)   moteur(-est);
  else if (r == R_PAUSE)   moteur(0);
  else if (r == R_APPROCHE) {
    homeOublierDetection();
    sensVerifie = false;
    azDebutConnu = azPourPosition(codeurServicePosition(), azDebutApproche);
    moteur(est);
  }
}

static void homeTrouve(int32_t posHome) {
  moteur(0);
  etatHorizontal = "STOP (HOME)";
  rech = R_AUCUNE;

  if (sessKOk) {
    // Calibration en cours : l'azimut du HOME est calcule avec les
    // deux points boussole, puis le HOME devient la reference.
    azPourPosition(posHome, sessAzHome);
    sessHomeOk = true;
    posRef = posHome; azRef = sessAzHome; kRef = sessK; sourceRef = REF_HOME;
    message = "HOME trouve : azimut " + String(sessAzHome, 1) + " deg. Enregistre maintenant les butees.";
  } else if (enrValide) {
    posRef = posHome; azRef = enrAzHome; kRef = enrK; sourceRef = REF_HOME;
    message = "HOME trouve : azimut de reference " + String(enrAzHome, 1) + " deg. L'azimut est maintenant mesure par le codeur.";
  } else {
    message = "HOME trouve, mais aucune calibration : releve d'abord les points 1 et 2.";
  }
  Serial.println("Calibration : " + message);
}

static void updateRecherche() {
  if (rech == R_AUCUNE) return;
  unsigned long now = millis();

  if (now - rechDebut > CALIB_HOME_TIMEOUT_MS) {
    arreterRecherche("Recherche abandonnee : HOME non trouve en 3 min.");
    return;
  }

  // Moteur commande mais codeur immobile : moteur bloque ou non
  // alimente, codeur debranche... on coupe.
  if (rech != R_PAUSE) {
    surveillerBlocage(false);
    if (now - blocageDebut > CALIB_BLOCAGE_MS) {
      arreterRecherche("Arret : le codeur ne compte pas (moteur bloque, non alimente, ou codeur debranche).");
      return;
    }
  }

  switch (rech) {
    case R_DEGAGEMENT:
      if (!homeDetecte()) entrerPhase(R_DEGAGEMENT_FIN);
      break;
    case R_DEGAGEMENT_FIN:
      if (now - phaseDebut >= CALIB_DEGAGEMENT_MS) entrerPhase(R_PAUSE);
      break;
    case R_PAUSE:
      // Arret franc avant d'inverser le sens : menage le reducteur.
      if (now - phaseDebut >= 500) entrerPhase(R_APPROCHE);
      break;
    case R_APPROCHE: {
      int32_t posHome;
      if (homeLireDetection(posHome)) { homeTrouve(posHome); return; }

      // Verification du sens : vers l'EST, l'azimut doit DIMINUER.
      double az;
      if (!sensVerifie && azDebutConnu && azPourPosition(codeurServicePosition(), az)) {
        double d = az - azDebutApproche;
        if (fabs(d) >= CALIB_CONTROLE_SENS_DEG) {
          if (d > 0) {
            arreterRecherche("Arret : la commande choisie pour l'EST fait tourner vers l'OUEST. Corrige l'etape 1.");
            return;
          }
          sensVerifie = true;
        }
      }
      break;
    }
    default: break;
  }
}

// ============================================================
// BUTEES LOGICIELLES (calibration validee et HOME trouve)
// ============================================================

static void appliquerButees() {
  if (rech != R_AUCUNE || !calibAzimutDisponible()) return;
  int8_t sens = sensAzimut();
  if (sens == 0) return;
  double az = calibAzimutDeg();
  bool versEst = (sens == enrSensEst);
  if (versEst && az <= enrButEst) {
    commandeStopHorizontal();
    message = "Butee EST atteinte (" + String(enrButEst, 1) + " deg).";
  } else if (!versEst && az >= enrButOuest) {
    commandeStopHorizontal();
    message = "Butee OUEST atteinte (" + String(enrButOuest, 1) + " deg).";
  }
}

// ============================================================
// API PUBLIQUE
// ============================================================

void setupCalibAzimut() {
  if (!prefsAz.begin("trackazim", false)) {
    Serial.println("Calibration azimut : NVS indisponible.");
    return;
  }
  enrValide   = prefsAz.getBool("valide", false);
  enrK        = prefsAz.getDouble("k", 0);
  enrAzHome   = prefsAz.getDouble("azHome", 0);
  enrButEst   = prefsAz.getDouble("butEst", 0);
  enrButOuest = prefsAz.getDouble("butOuest", 0);
  enrSensEst  = prefsAz.getChar("sensEst", 0);
  if (enrValide && (enrK == 0 || enrSensEst == 0)) enrValide = false;
  Serial.printf("Calibration azimut : %s\n", enrValide ? "enregistree (HOME a rechercher)" : "aucune");
}

void updateCalibAzimut() {
  updateRecherche();
  appliquerButees();
}

bool calibAzimutDisponible() {
  return enrValide && sourceRef == REF_HOME;
}

double calibAzimutDeg() {
  double az = 0;
  azPourPosition(codeurServicePosition(), az);
  return az;
}

void calibAnnulerRecherche() {
  if (rech == R_AUCUNE) return;
  rech = R_AUCUNE;
  message = "Recherche du HOME interrompue par une commande manuelle.";
}

String calibChoisirSensEst(int8_t sensMoteur) {
  if (sensMoteur != 1 && sensMoteur != -1) return "Sens invalide.";
  sessSensEst = sensMoteur;
  message = String("EST = bouton ") + (sensMoteur < 0 ? "GAUCHE" : "DROITE") + ".";
  return message;
}

String calibReleverPoint(int numero, double azBoussole) {
  if (!codeurServiceOK())           return message = "Codeur indisponible.";
  if (rech != R_AUCUNE)             return message = "Attends la fin de la recherche du HOME.";
  if (isnan(azBoussole) || azBoussole < 0 || azBoussole >= 360)
                                    return message = "Azimut invalide : entre une valeur de 0 a 359.";
  int32_t pos = codeurServicePosition();

  if (numero == 1) {
    p1 = pos; az1 = azBoussole; p1Ok = true;
    // Un nouveau point 1 rend caducs tous les resultats qui en decoulent.
    p2Ok = sessKOk = ctrlOk = sessHomeOk = butEstOk = butOuestOk = false;
    if (sourceRef == REF_POINTS) sourceRef = REF_AUCUNE;
    return message = "Point 1 releve : " + String(azBoussole, 1) + " deg, codeur " + String(pos) + ".";
  }

  if (numero == 2) {
    if (!p1Ok) return message = "Releve d'abord le point 1.";
    if (fabs(azBoussole - az1) < CALIB_ECART_MIN_DEG)
      return message = "Ecart trop faible avec le point 1 : il faut au moins " + String((int)CALIB_ECART_MIN_DEG) + " deg.";
    if (abs(pos - p1) < 100)
      return message = "Le codeur n'a presque pas bouge depuis le point 1 : le tracker a-t-il tourne ?";
    p2 = pos; az2 = azBoussole; p2Ok = true;
    sessK = (double)(p2 - p1) / (az2 - az1);
    sessKOk = true;
    ctrlOk = sessHomeOk = butEstOk = butOuestOk = false;
    posRef = p1; azRef = az1; kRef = sessK; sourceRef = REF_POINTS;
    return message = "Point 2 releve : " + String(fabs(sessK), 1) + " pas par degre d'azimut.";
  }

  if (numero == 3) {
    double az;
    if (!azPourPosition(pos, az)) return message = "Releve d'abord les points 1 et 2.";
    ctrlBoussole = azBoussole; ctrlCalcule = az; ctrlOk = true;
    double ecart = az - azBoussole;
    return message = "Controle : calcule " + String(az, 1) + " deg, boussole " + String(azBoussole, 1)
                   + " deg, ecart " + String(ecart, 1) + " deg"
                   + (fabs(ecart) <= 3 ? " : bon." : " : trop grand, refais les points 1 et 2.");
  }
  return message = "Point inconnu.";
}

String calibLancerRechercheHome() {
  if (!codeurServiceOK())   return message = "Codeur indisponible.";
  if (sensEstActif() == 0)  return message = "Indique d'abord quel bouton tourne vers l'EST (etape 1).";
  if (rech != R_AUCUNE)     return message = "Recherche deja en cours.";

  commandeStopHorizontal();    // annule l'arret auto d'une commande manuelle
  etatHorizontal = "RECHERCHE HOME";
  rechDebut = millis();
  entrerPhase(homeDetecte() ? R_DEGAGEMENT : R_APPROCHE);
  return message = "Recherche du HOME lancee (vers l'EST).";
}

String calibArreterRecherche() {
  if (rech == R_AUCUNE) { commandeStopHorizontal(); return message = "Arret."; }
  arreterRecherche("Recherche du HOME arretee.");
  return message;
}

String calibButee(bool coteEst) {
  double az;
  if (!azPourPosition(codeurServicePosition(), az)) return message = "Releve d'abord les points 1 et 2.";
  if (coteEst) { butEst = az; butEstOk = true; }
  else         { butOuest = az; butOuestOk = true; }
  return message = String("Butee ") + (coteEst ? "EST" : "OUEST") + " : " + String(az, 1) + " deg.";
}

String calibValider() {
  if (sessSensEst == 0)        return message = "Etape 1 manquante : bouton vers l'EST.";
  if (!sessKOk)                return message = "Points 1 et 2 manquants.";
  if (!sessHomeOk)             return message = "Recherche du HOME manquante.";
  if (!butEstOk || !butOuestOk) return message = "Butees EST et OUEST manquantes.";
  if (butOuest - butEst < 10)  return message = "Butees incoherentes : la butee OUEST doit etre au moins 10 deg apres la butee EST.";

  enrK = sessK; enrAzHome = sessAzHome; enrButEst = butEst; enrButOuest = butOuest;
  enrSensEst = sessSensEst; enrValide = true;
  prefsAz.putDouble("k", enrK);
  prefsAz.putDouble("azHome", enrAzHome);
  prefsAz.putDouble("butEst", enrButEst);
  prefsAz.putDouble("butOuest", enrButOuest);
  prefsAz.putChar("sensEst", enrSensEst);
  prefsAz.putBool("valide", true);
  return message = "Calibration enregistree : l'azimut affiche est maintenant mesure par le codeur.";
}

String calibEffacer() {
  prefsAz.clear();
  enrValide = false; enrK = enrAzHome = enrButEst = enrButOuest = 0; enrSensEst = 0;
  sessSensEst = 0;
  p1Ok = p2Ok = sessKOk = ctrlOk = sessHomeOk = butEstOk = butOuestOk = false;
  sourceRef = REF_AUCUNE;
  return message = "Calibration effacee : retour a l'azimut estime.";
}

String calibJson() {
  JsonDocument doc;
  int32_t pos = codeurServicePosition();
  doc["codeurOK"] = codeurServiceOK();
  doc["pos"]      = pos;
  doc["home"]     = homeDetecte();
  double az;
  if (azPourPosition(pos, az)) doc["az"] = az; else doc["az"] = nullptr;
  doc["source"]     = sourceRef == REF_HOME ? "home" : sourceRef == REF_POINTS ? "points" : "aucune";
  doc["disponible"] = calibAzimutDisponible();
  doc["etatH"]      = etatHorizontal;
  doc["recherche"]  = nomRecherche();
  doc["message"]    = message;
  doc["sensEst"]    = sensEstActif();

  JsonObject s = doc["session"].to<JsonObject>();
  s["p1"] = p1Ok; if (p1Ok) { s["az1"] = az1; s["pos1"] = p1; }
  s["p2"] = p2Ok; if (p2Ok) { s["az2"] = az2; s["pos2"] = p2; }
  if (sessKOk) s["k"] = sessK;
  s["ctrl"] = ctrlOk; if (ctrlOk) { s["ctrlBoussole"] = ctrlBoussole; s["ctrlCalcule"] = ctrlCalcule; }
  s["homeOk"] = sessHomeOk; if (sessHomeOk) s["azHome"] = sessAzHome;
  s["butEstOk"] = butEstOk;     if (butEstOk) s["butEst"] = butEst;
  s["butOuestOk"] = butOuestOk; if (butOuestOk) s["butOuest"] = butOuest;

  JsonObject e = doc["enregistree"].to<JsonObject>();
  e["valide"] = enrValide;
  if (enrValide) {
    e["k"] = enrK; e["azHome"] = enrAzHome;
    e["butEst"] = enrButEst; e["butOuest"] = enrButOuest; e["sensEst"] = enrSensEst;
  }

  String out;
  serializeJson(doc, out);
  return out;
}
