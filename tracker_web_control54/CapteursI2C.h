// ============================================================
// CapteursI2C.h - BMP280 (pression/temperature) + RTC DS3231
// ------------------------------------------------------------
// Les deux capteurs partagent le meme bus I2C (SDA 18 / SCL 19)
// et sont alimentes UNIQUEMENT par le 3.3V de l'ESP32.
// Le BMP280 est un clone HW-611 : il NE mesure PAS l'humidite.
// ============================================================

#pragma once

#include <Arduino.h>

// Demarre le bus I2C et detecte les deux capteurs.
// Retourne true si au moins l'un des deux a repondu.
bool setupCapteursI2C();

// Regle la RTC depuis un serveur NTP. A appeler une seule fois,
// apres la connexion WiFi. Sans effet si la RTC est absente.
void synchroniserRTCDepuisNTP();

bool bmp280Present();
bool rtcPresente();

float lirePressionLocaleHpa();
float lireTemperatureLocaleC();

// Ecrit "AAAA-MM-JJ hh:mm:ss" dans buf. Retourne false si RTC absente.
bool lireHeureRTC(char* buf, size_t taille);

// Composants de la date/heure RTC, en UTC (la synchro NTP regle la
// RTC en UTC). Retourne false si la RTC est absente.
bool lireDateHeureRTC(int& annee, int& mois, int& jour,
                      int& heure, int& minute, int& seconde);

// AJOUT 2 : secondes depuis le 1er janvier 1970 (UTC), pour horodater
// les barres du bargraphe des rafales. Lecture seule de la RTC.
// Retourne false si la RTC est absente.
bool lireEpochRTC(uint32_t& epoch);
