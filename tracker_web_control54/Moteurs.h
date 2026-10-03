// ============================================================
// Moteurs.h - Pilotage des 2 ponts en H BTS7960 (RPWM / LPWM)
// ============================================================

#pragma once

#include <Arduino.h>

// Configure les 4 canaux PWM et met les deux axes a l'arret.
void setupMoteurs();

// sens : +1 / -1 / 0 (arret)   vitesse : 0..255
void driveInclinaison(int8_t sens, uint8_t vitesse);
void driveAzimut(int8_t sens, uint8_t vitesse);
