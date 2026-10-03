// ============================================================
// CalibAzimut.h - Calibration de l'azimut par le codeur + HOME
// ------------------------------------------------------------
// Le codeur est sur l'arbre moteur et le rapport de reduction est
// inconnu : on mesure directement les PAS PAR DEGRE d'azimut a
// partir de deux lectures de boussole (nord geographique).
//
//   azimut = azRef + (position codeur - posRef) / pasParDeg
//
// Le codeur repart a 0 a chaque demarrage : la reference est
// retrouvee en allant chercher le capteur HOME (cote EST), dont
// l'azimut est enregistre pendant la calibration.
//
// Rien ne change pour le reste du programme tant que la calibration
// n'est pas VALIDEE et que le HOME n'a pas ete trouve depuis le
// demarrage : l'azimut affiche reste alors l'estimation par le temps
// moteur. Le suivi automatique ne pilote pas l'azimut.
// ============================================================

#pragma once

#include <Arduino.h>

// Charge la calibration enregistree (NVS).
void setupCalibAzimut();

// Recherche du HOME + butees logicielles. A appeler a chaque loop().
void updateCalibAzimut();

// true : calibration validee ET HOME trouve depuis le demarrage.
bool calibAzimutDisponible();

// Azimut mesure par le codeur (degres, nord geographique).
// N'a de sens que si calibAzimutDisponible().
double calibAzimutDeg();

// Une commande manuelle de l'azimut interrompt la recherche du HOME.
void calibAnnulerRecherche();

// --- Etapes de la page /calibration (renvoient un message) ---
String calibChoisirSensEst(int8_t sensMoteur);   // -1 = GAUCHE, +1 = DROITE
String calibReleverPoint(int numero, double azBoussole);  // 1, 2, ou 3 (controle)
String calibLancerRechercheHome();
String calibArreterRecherche();
String calibButee(bool cote_est);
String calibValider();
String calibEffacer();

String calibJson();
