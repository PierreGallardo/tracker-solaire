// ============================================================
// Azimut.h - Position azimutale
// ------------------------------------------------------------
// Par defaut ESTIMEE : integree a partir du temps de commande moteur.
// MESUREE par le codeur Baumer une fois la calibration validee et le
// capteur HOME trouve depuis le demarrage (voir CalibAzimut.h).
// ============================================================

#pragma once

#include <Arduino.h>

// Initialise l'horodatage de l'integration.
void setupAzimut();

// Integre la position en fonction du sens de commande courant.
// A appeler a chaque tour de loop().
void updateEstimationAzimut();

// Declare le sens moteur en cours : +1 (droite), -1 (gauche), 0 (arret).
void setSensAzimut(int8_t sens);

// Sens moteur en cours : +1, -1 ou 0.
int8_t sensAzimut();

// Azimut ramene dans [0 ; 360[ : MESURE par le codeur si la calibration
// est validee et le HOME trouve depuis le demarrage, sinon ESTIME.
double azimutNormaliseDeg();

// true si azimutNormaliseDeg() vient du codeur.
bool azimutMesure();
