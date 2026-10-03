#include "Controle.h"
#include "Config.h"
#include "Moteurs.h"
#include "Inclinaison.h"
#include "Azimut.h"
#include "Vent.h"
#include "Solaire.h"
#include "CalibAzimut.h"

#include <math.h>

String etatVertical   = "STOP";
String etatHorizontal = "STOP";

static unsigned long arretAutoVerticalAt   = 0;
static unsigned long arretAutoHorizontalAt = 0;

static bool retourAPlatActif = false;
static bool suiviAuto        = false;

// ============================================================
// TEMPS RESTANT AVANT ARRET AUTOMATIQUE
// ============================================================

static long secondesRestantes(unsigned long echeance) {
  long sec = (echeance != 0) ? (long)((echeance - millis()) / 1000) : 0;
  if (sec < 0) sec = 0;
  return sec;
}

long secondesRestantesVertical()   { return secondesRestantes(arretAutoVerticalAt); }
long secondesRestantesHorizontal() { return secondesRestantes(arretAutoHorizontalAt); }

// ============================================================
// COMMANDES - AXE VERTICAL (inclinaison)
// ============================================================

void commandeMonter() {
  suiviAuto = false;   // une commande manuelle sort du mode auto
  // Palier "vent modere" : on refuse de deployer au-dela du plafond.
  // Le retour a plat eventuellement en cours n'est PAS annule.
  if (ventModeReduit() && lireInclinaisonReelle() >= ANGLE_MAX_VENT_REDUIT_DEG) {
    driveInclinaison(1, 0);
    arretAutoVerticalAt = 0;
    etatVertical = "REFUSE (vent : max " + String((int)ANGLE_MAX_VENT_REDUIT_DEG) + "°)";
    Serial.printf("Commande MONTER refusee : vent modere, plafond %.0f deg atteint\n",
                  (double)ANGLE_MAX_VENT_REDUIT_DEG);
    return;
  }

  retourAPlatActif = false;
  etatVertical = "MONTER";
  driveInclinaison(1, ventVitesseMoteur());
  arretAutoVerticalAt = millis() + DUREE_MAX_MS;
  Serial.println("Commande reçue : MONTER");
}

void commandeStopVertical() {
  suiviAuto = false;   // une commande manuelle sort du mode auto
  retourAPlatActif = false;
  etatVertical = "STOP";
  driveInclinaison(1, 0);
  arretAutoVerticalAt = 0;
  Serial.println("Commande reçue : STOP (vertical)");
}

void commandeDescendre() {
  suiviAuto = false;   // une commande manuelle sort du mode auto
  retourAPlatActif = false;
  etatVertical = "DESCENDRE";
  driveInclinaison(-1, ventVitesseMoteur());
  arretAutoVerticalAt = millis() + DUREE_MAX_MS;
  Serial.println("Commande reçue : DESCENDRE");
}

void commandeRetourAPlat() {
  suiviAuto = false;   // une commande manuelle sort du mode auto
  retourAPlatActif = true;
  etatVertical = "RETOUR A PLAT";
  arretAutoVerticalAt = 0;
  Serial.println("Commande reçue : RETOUR A PLAT");
}

// ============================================================
// COMMANDES - AXE HORIZONTAL (azimut)
// ============================================================

void commandeGauche() {
  calibAnnulerRecherche();
  etatHorizontal = "GAUCHE";
  driveAzimut(-1, ventVitesseMoteur());
  setSensAzimut(-1);
  arretAutoHorizontalAt = millis() + DUREE_MAX_MS;
  Serial.println("Commande reçue : GAUCHE");
}

void commandeStopHorizontal() {
  calibAnnulerRecherche();
  etatHorizontal = "STOP";
  driveAzimut(1, 0);
  setSensAzimut(0);
  arretAutoHorizontalAt = 0;
  Serial.println("Commande reçue : STOP (horizontal)");
}

void commandeDroite() {
  calibAnnulerRecherche();
  etatHorizontal = "DROITE";
  driveAzimut(1, ventVitesseMoteur());
  setSensAzimut(1);
  arretAutoHorizontalAt = millis() + DUREE_MAX_MS;
  Serial.println("Commande reçue : DROITE");
}

// ============================================================
// MODE SUIVI SOLAIRE AUTOMATIQUE
// ============================================================

void activerSuiviAuto() {
  suiviAuto = true;
  retourAPlatActif = false;
  arretAutoVerticalAt = 0;   // le suivi auto gere lui-meme ses arrets
  Serial.println("Commande reçue : SUIVI SOLAIRE AUTO");
}

void desactiverSuiviAuto() {
  suiviAuto = false;
  driveInclinaison(1, 0);
  etatVertical = "STOP";
  Serial.println("Commande reçue : MODE MANUEL");
}

bool suiviAutoActif() { return suiviAuto; }

double cibleSuiviAutoDeg() {
  double cible = inclinaisonCibleSolaire();
  // Le palier "vent modere" plafonne aussi le suivi automatique.
  if (ventModeReduit() && cible > ANGLE_MAX_VENT_REDUIT_DEG) {
    cible = ANGLE_MAX_VENT_REDUIT_DEG;
  }
  return cible;
}

// ============================================================
// BOUCLE DE CONTROLE
// ============================================================

void updateControle() {
  // Arret automatique de securite : aucune commande manuelle ne
  // peut faire tourner un moteur plus de DUREE_MAX_MS.
  if (arretAutoVerticalAt != 0 && millis() > arretAutoVerticalAt) {
    etatVertical = "STOP";
    driveInclinaison(1, 0);
    arretAutoVerticalAt = 0;
    Serial.println("Arret automatique (securite vertical)");
  }
  if (arretAutoHorizontalAt != 0 && millis() > arretAutoHorizontalAt) {
    etatHorizontal = "STOP";
    driveAzimut(1, 0);
    setSensAzimut(0);
    arretAutoHorizontalAt = 0;
    Serial.println("Arret automatique (securite horizontal)");
  }

  // Asservissement du retour a plat sur la mesure du potentiometre.
  static unsigned long lastRetourCheck = 0;
  if (retourAPlatActif && millis() - lastRetourCheck >= RETOUR_A_PLAT_PERIODE_MS) {
    lastRetourCheck = millis();
    double actuel = lireInclinaisonReelle();
    double erreur = 0.0 - actuel;

    if (fabs(erreur) < RETOUR_A_PLAT_TOLERANCE_DEG) {
      driveInclinaison(1, 0);
      retourAPlatActif = false;
      etatVertical = "STOP (a plat)";
      Serial.println("Retour a plat termine.");
    } else {
      int8_t sens = (erreur > 0) ? 1 : -1;
      driveInclinaison(sens, RETOUR_A_PLAT_VITESSE);
    }
  }

  // Suivi solaire automatique de l'inclinaison.
  // Suspendu tant qu'un retour a plat est en cours : la securite
  // vent est prioritaire. Le suivi reprend seul ensuite.
  static unsigned long lastAutoCheck = 0;
  if (suiviAuto && !retourAPlatActif && millis() - lastAutoCheck >= SUIVI_AUTO_PERIODE_MS) {
    lastAutoCheck = millis();
    double cible  = cibleSuiviAutoDeg();
    double actuel = lireInclinaisonReelle();
    double erreur = cible - actuel;

    if (fabs(erreur) < SUIVI_AUTO_TOLERANCE_DEG) {
      driveInclinaison(1, 0);   // zone morte : on ne pompe pas sur le verin
      etatVertical = soleilLeve() ? "AUTO (en position)" : "AUTO (nuit, a plat)";
    } else {
      int8_t sens = (erreur > 0) ? 1 : -1;
      driveInclinaison(sens, SUIVI_AUTO_VITESSE);
      etatVertical = "AUTO (poursuite " + String(cible, 1) + "°)";
    }
  }
}

// Securite vent : retour a plat automatique si vent trop fort
void appliquerSecuriteVent() {
  if (ventSecuriteActive() && !retourAPlatActif) {
    retourAPlatActif = true;
    etatVertical = "RETOUR A PLAT (VENT)";
    arretAutoVerticalAt = 0;
  }
}
