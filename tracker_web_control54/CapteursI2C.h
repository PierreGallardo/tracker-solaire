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

// Demarre l'heure systeme NTP au fuseau Europe/Paris (heure d'ete
// automatique, resynchronisee toutes les heures par le SNTP), puis
// regle la RTC en UTC si elle est presente. A appeler une seule
// fois, apres la connexion WiFi.
void synchroniserRTCDepuisNTP();

// --- Heure systeme NTP (affichage uniquement) ----------------
// La RTC reste la source de temps du calcul solaire, des
// historiques et du bargraphe (en UTC) ; l'affichage utilise
// l'heure NTP, en heure de Paris.

// true une fois la premiere synchronisation NTP obtenue.
bool heureNTPValide();

// Secondes UTC depuis 1970 (heure systeme NTP). 0 si non valide.
uint32_t epochNTP();

// Decalage de l'heure de Paris sur l'UTC, en secondes (3600 en
// hiver, 7200 en ete), tire de la regle POSIX du fuseau.
long decalageParisSecondes();

// Ecrit l'heure de Paris dans buf selon le format strftime donne
// (%d, %m, %Y, %H, %M, %S...). Retourne false si l'heure NTP n'est
// pas encore valide.
bool formaterHeureParis(char* buf, size_t taille, const char* format);

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
