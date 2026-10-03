#include "Azimut.h"
#include "Config.h"
#include "CalibAzimut.h"

#include <math.h>

static double        azimutEstimeDeg   = 0.0;
static unsigned long lastAzimutUpdate  = 0;
static int8_t        sensAzimutActuel  = 0;

void setupAzimut() {
  lastAzimutUpdate = millis();
}

void updateEstimationAzimut() {
  unsigned long now = millis();
  double dt = (now - lastAzimutUpdate) / 1000.0;
  lastAzimutUpdate = now;
  if (sensAzimutActuel != 0) {
    azimutEstimeDeg += VITESSE_AZIMUT_DEG_S * sensAzimutActuel * dt;
  }
}

void setSensAzimut(int8_t sens) {
  sensAzimutActuel = sens;
}

int8_t sensAzimut() {
  return sensAzimutActuel;
}

bool azimutMesure() {
  return calibAzimutDisponible();
}

double azimutNormaliseDeg() {
  double az = azimutMesure() ? calibAzimutDeg() : azimutEstimeDeg;
  double azNorm = fmod(az, 360.0);
  if (azNorm < 0) azNorm += 360.0;
  return azNorm;
}
