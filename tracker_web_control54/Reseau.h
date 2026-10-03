// ============================================================
// Reseau.h - Connexion WiFi + publication mDNS
// ============================================================

#pragma once

#include <Arduino.h>

// Se connecte au WiFi (timeout 15 s). Retourne false en cas d'echec.
bool connectWiFi();

// Publie le nom MDNS_HOSTNAME -> http://tracker.local
void demarrerMDNS();

// --- Flashage Over-The-Air (via WiFi, sans cable USB) -------
// Une fois actif, l'ESP32 apparait dans le selecteur de port de
// l'Arduino IDE comme port reseau, sur le port 3232.
//
// FILET DE SECURITE : si le code flashe casse le WiFi ou le boot,
// l'OTA devient inaccessible et il faut repasser par l'USB.
// Garder le cable a portee de main sur le terrain.
void setupOTA();

// Traite les demandes de flashage. A appeler au tout debut de loop().
void gererOTA();
