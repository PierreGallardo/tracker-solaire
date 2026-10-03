#include "Inclinaison.h"
#include "Config.h"

// Moyenne glissante simple sur NB_ECHANTILLONS_POT lectures ADC.
static int lireBrutFiltre(int pin) {
  long somme = 0;
  for (int i = 0; i < NB_ECHANTILLONS_POT; i++) { somme += analogRead(pin); delay(1); }
  return somme / NB_ECHANTILLONS_POT;
}

// Interpolation lineaire entre les deux points de calibration mesures.
static double brutVersDegres(int brut, int brut0Deg, int brut40Deg) {
  return (double)(brut - brut0Deg) * POT_ANGLE_MAX_DEG / (double)(brut40Deg - brut0Deg);
}

double lireInclinaisonReelle() {
  int brutA = lireBrutFiltre(PIN_POT_CANAL_A);
  int brutB = lireBrutFiltre(PIN_POT_CANAL_B);
  double degA = brutVersDegres(brutA, POT_A_0DEG, POT_A_40DEG);
  double degB = brutVersDegres(brutB, POT_B_0DEG, POT_B_40DEG);
  return (degA + degB) / 2.0;
}
