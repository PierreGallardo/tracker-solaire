// ============================================================
// Secrets.exemple.h - MODELE de Secrets.h
// ------------------------------------------------------------
// Copier ce fichier en "Secrets.h" (meme dossier), puis remplacer
// chaque valeur. Secrets.h est ignore par Git : les mots de passe
// et la position du tracker ne sont jamais publies.
// ============================================================

#pragma once

// --- WiFi ---
#define WIFI_SSID       "NOM_DU_RESEAU"
#define WIFI_PASSWORD   "MOT_DE_PASSE_WIFI"

// --- Flashage OTA (au moins 8 caracteres) ---
#define OTA_PASSWORD    "MOT_DE_PASSE_OTA"

// --- Position du tracker (meteo en ligne / calcul solaire) ---
#define LATITUDE_DEG    0.0000   // degres, nord positif
#define LONGITUDE_DEG   0.0000   // degres, est positif
#define ALTITUDE_M      0        // metres
