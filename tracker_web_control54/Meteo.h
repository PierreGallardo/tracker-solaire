// ============================================================
// Meteo.h - Meteo regionale en ligne (Open-Meteo, sans cle API)
// ============================================================

#pragma once

#include <Arduino.h>

struct MeteoEnLigne {
  float temperatureC   = 0;
  float ventKmh        = 0;
  float rafaleKmh      = 0;   // rafales a 10 m (comparaison avec l anemometre)
  float pressionHpa    = 0;
  int   probaPluiePct  = 0;
  bool  valide         = false;
  unsigned long derniereMaj = 0;
};

extern MeteoEnLigne meteo;

// Interroge l'API immediatement (bloquant, ~1 s).
void fetchMeteo();

// Relance fetchMeteo() une fois METEO_INTERVAL_MS ecoule.
// A appeler a chaque tour de loop().
void updateMeteo();
