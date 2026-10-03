// ============================================================
// HistoMeteo.h - Historique local temperature / pression
// ------------------------------------------------------------
// Releve le BMP280 toutes les PERIODE_HISTO_METEO_MS (30 min) et
// conserve NB_ECHANT_HISTO_METEO points, soit 12 h glissantes.
//
// Expose une tendance "up" / "down" / "stable" pour chacune des
// deux grandeurs, calculee comme celle du vent : moyenne de la
// premiere moitie du buffer contre celle de la seconde.
//
// Memoire vive uniquement : l'historique repart de zero a chaque
// redemarrage (contrairement a celui des mises en securite vent,
// qui est persiste en NVS).
// ============================================================

#pragma once

#include <Arduino.h>

// Premier releve immediat, pour ne pas afficher une case vide
// pendant la premiere demi-heure.
void setupHistoMeteo();

// Ajoute un point si la periode est ecoulee. A appeler dans loop().
void updateHistoMeteo();

// "up" (hausse) / "down" (baisse) / "stable".
const String& tendanceTemperature();
const String& tendancePression();

// [{"h":"14:30","t":23.7,"p":937.8}, ...], du plus ancien au plus
// recent. "[]" tant qu'aucun releve n'a ete fait.
String historiqueMeteoJson();

// Meme contenu au format tableur (separateur ";", virgule decimale,
// pour une ouverture directe dans Excel / LibreOffice en francais) :
// heure_utc;temperature_C;pression_hPa
String historiqueMeteoCsv();
