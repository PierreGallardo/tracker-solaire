#include "Solaire.h"
#include "Config.h"
#include "CapteursI2C.h"

#include <math.h>

// ============================================================
// CONVERSION CALENDRIER -> JOUR LINEAIRE
// (algorithme Howard Hinnant, standard et compact)
// ============================================================

static long daysFromCivil(int y, int m, int d) {
  y -= (m <= 2);
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe - 719468L;
}

// ============================================================
// POSITION SOLAIRE (algorithme NOAA)
// ============================================================

void computeSunPosition(int year, int month, int day,
                        int hour, int minute, int second,
                        double latDeg, double lonDeg, double utcOffsetHours,
                        double& azimuthDeg, double& elevationDeg) {
  int dayOfYear = (int)(daysFromCivil(year, month, day) - daysFromCivil(year, 1, 1)) + 1;

  double gamma = 2.0 * PI / 365.0 * (dayOfYear - 1 + (hour - 12) / 24.0);

  double eqtime = 229.18 * (0.000075 + 0.001868 * cos(gamma) - 0.032077 * sin(gamma)
                  - 0.014615 * cos(2 * gamma) - 0.040849 * sin(2 * gamma));

  double decl = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma)
              - 0.006758 * cos(2 * gamma) + 0.000907 * sin(2 * gamma)
              - 0.002697 * cos(3 * gamma) + 0.00148 * sin(3 * gamma); // radians

  double timeOffset = eqtime + 4.0 * lonDeg - 60.0 * utcOffsetHours; // minutes
  double tst = hour * 60.0 + minute + second / 60.0 + timeOffset;    // minutes

  double ha = tst / 4.0 - 180.0; // degres
  double haRad = ha * PI / 180.0;
  double latRad = latDeg * PI / 180.0;

  double cosZenith = sin(latRad) * sin(decl) + cos(latRad) * cos(decl) * cos(haRad);
  cosZenith = constrain(cosZenith, -1.0, 1.0);
  double zenithRad = acos(cosZenith);

  elevationDeg = 90.0 - zenithRad * 180.0 / PI;

  double cosAz = (sin(latRad) * cos(zenithRad) - sin(decl)) / (cos(latRad) * sin(zenithRad));
  cosAz = constrain(cosAz, -1.0, 1.0);
  double azBase = acos(cosAz) * 180.0 / PI;

  if (ha > 0) {
    azimuthDeg = fmod(azBase + 180.0, 360.0);
  } else {
    azimuthDeg = fmod(540.0 - azBase, 360.0);
  }
}

// ============================================================
// APPLICATION AU TRACKER
// ============================================================

bool positionSolaire(double& azimutDeg, double& elevationDeg) {
  int y, mo, d, h, mi, s;
  if (!lireDateHeureRTC(y, mo, d, h, mi, s)) return false;

  // La RTC est en UTC -> decalage nul.
  computeSunPosition(y, mo, d, h, mi, s,
                     LATITUDE_DEG, LONGITUDE_DEG, 0.0,
                     azimutDeg, elevationDeg);
  return true;
}

bool soleilLeve() {
  double az, elev;
  if (!positionSolaire(az, elev)) return false;
  return elev >= ELEVATION_MIN_SUIVI_DEG;
}

double inclinaisonCibleSolaire() {
  double az, elev;
  if (!positionSolaire(az, elev)) return 0.0;          // pas de RTC -> repos a plat
  if (elev < ELEVATION_MIN_SUIVI_DEG) return 0.0;      // nuit      -> repos a plat

  // Panneau perpendiculaire aux rayons : plus le soleil est bas,
  // plus le panneau doit etre redresse.
  double cible = 90.0 - elev;

  // Bornage a la course mecanique reelle du verin.
  if (cible < 0.0) cible = 0.0;
  if (cible > POT_ANGLE_MAX_DEG) cible = POT_ANGLE_MAX_DEG;
  return cible;
}
