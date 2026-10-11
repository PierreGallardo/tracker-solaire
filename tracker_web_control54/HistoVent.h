// ============================================================
// HistoVent.h - Historique local du vent (anemometre)
// ------------------------------------------------------------
// Meme fonctionnement que HistoMeteo (temperature / pression) :
// un point toutes les PERIODE_HISTO_VENT_MS (30 min), 24 points
// soit 12 h glissantes, conserves en NVS, tendance, export CSV.
//
// Chaque point resume la demi-heure ecoulee :
//   - vent MOYEN  : moyenne du vent (fenetre 5 s) echantillonne
//                   toutes les 5 s ;
//   - RAFALE MAX  : plus forte rafale (pic sur 2 s) de la demi-heure.
//
// Independant de l'historique des MISES EN SECURITE (Vent.cpp), qui
// ne garde que les evenements.
// ============================================================

#pragma once

#include <Arduino.h>

// Charge l'historique enregistre et ouvre la premiere demi-heure.
void setupHistoVent();

// Accumule le vent et ajoute un point toutes les 30 min. A appeler
// dans loop().
void updateHistoVent();

// "up" / "down" / "stable", calculee sur le vent moyen.
const String& tendanceHistoVent();

// {"points":[{"h":"14:30","m":8.2,"r":23.4}, ...],   du plus ancien au plus recent
//  "tendance":"up",
//  "enCours":{"min":12,"m":6.1,"r":18.0}}           demi-heure en cours
String historiqueVent30Json();

// heure_utc;vent_moyen_kmh;rafale_max_kmh  (";" et virgule decimale)
String historiqueVent30Csv();
