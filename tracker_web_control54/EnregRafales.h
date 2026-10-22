// ============================================================
// EnregRafales.h - AJOUT 2 : enregistrement des rafales (bargraphe)
// ------------------------------------------------------------
// - Demarre des qu'un echantillon depasse ENREG_DEMARRAGE_KMH (7).
// - Une barre toutes les ENREG_PAS_MS (15 s) = vitesse MAX de
//   l'intervalle, horodatee par la RTC (UTC).
// - S'arrete apres ENREG_ARRET_CALME_MS (30 min) consecutives sous le
//   seuil ; tout echantillon au-dessus relance le decompte.
// - Tampon circulaire de ENREG_NB_BARRES (5760 = 24 h de barres).
// - Sauvegarde dans LittleFS toutes les ENREG_SAUVEGARDE_MS (5 min)
//   si de nouvelles barres sont arrivees, ecrite par morceaux de
//   quelques centaines d'octets a chaque tour de loop() pour ne pas
//   bloquer le reste du programme ; rechargement au demarrage.
//
// Pourquoi LittleFS et pas NVS : 5760 barres de 6 octets = 34 560
// octets. La partition NVS du schema min_spiffs ne fait que 20 Ko au
// total (dont une page reservee), deja partagee avec les historiques
// vent, meteo, calibration et releves. La partition "spiffs" (128 Ko)
// n'etait pas utilisee : elle accueille le fichier.
// ============================================================

#pragma once

#include <Arduino.h>

// Monte LittleFS (formate la partition au premier demarrage) et
// recharge les barres sauvegardees. A appeler dans setup().
void setupEnregRafales();

// Appelee par Vent.cpp pour chaque echantillon de vitesse (100 ms).
void enregRafalesEchantillon(float kmh);

// Fermeture des barres, arret de l'enregistrement, sauvegarde par
// morceaux. A appeler a chaque tour de loop().
void updateEnregRafales();

bool enregRafalesActif();

// Fonction d'ecriture d'un morceau de la reponse HTTP.
typedef void (*EcrireMorceau)(const char* texte);

// Serialise en JSON, par morceaux d'environ 1 Ko, les barres dont
// l'heure est strictement posterieure a `depuis` (0 = les dernieres
// 24 h). Les barres consecutives (15 s d'ecart) sont regroupees en
// segments : {"segments":[[heureDebut,[v10, v10, ...]], ...]} ou
// v10 = vitesse x 10 (entier). Une rupture d'heure = nouveau segment.
void enregRafalesJson(uint32_t depuis, EcrireMorceau ecrire);
