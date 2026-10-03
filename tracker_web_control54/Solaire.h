// ============================================================
// Solaire.h - Position theorique du soleil (algorithme NOAA)
// ------------------------------------------------------------
// Repris de sketch_aug26e.ino, avec une difference importante :
// la source de temps n'est plus l'heure de compilation, mais la
// RTC DS3231 synchronisee par NTP au demarrage.
//
// La RTC est reglee en UTC (voir synchroniserRTCDepuisNTP), donc
// le calcul se fait avec un decalage nul : aucun probleme
// d'heure d'ete / heure d'hiver a gerer.
//
// Precision de l'algorithme : environ 0.01 a 0.5 degre.
// ============================================================

#pragma once

#include <Arduino.h>

// Azimut et elevation du soleil pour l'instant courant.
// Retourne false si la RTC n'a pas repondu (valeurs inchangees).
bool positionSolaire(double& azimutDeg, double& elevationDeg);

// true si le soleil est au-dessus du seuil de suivi.
bool soleilLeve();

// Inclinaison de panneau visee pour l'instant courant, en degres.
// Perpendiculaire au soleil (90 - elevation), bornee a la course
// mecanique reelle du verin [0 ; POT_ANGLE_MAX_DEG].
// Renvoie 0 (position de repos, a plat) la nuit ou sans RTC.
double inclinaisonCibleSolaire();

// Calcul brut, sans dependance a la RTC. Expose pour pouvoir
// interroger une date/heure arbitraire (tests, previsions).
// heure/minute/seconde en UTC, utcOffsetHours = 0.
void computeSunPosition(int year, int month, int day,
                        int hour, int minute, int second,
                        double latDeg, double lonDeg, double utcOffsetHours,
                        double& azimuthDeg, double& elevationDeg);
