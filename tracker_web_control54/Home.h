// ============================================================
// Home.h - Capteur HOME azimut (SN04-N inductif NPN NO) - VERSION TEST
// ------------------------------------------------------------
// Lecture seule : etat du capteur, tension sur la broche et nombre
// de detections depuis la derniere remise a zero. Non relie a
// l'azimut tant que la calibration n'est pas faite.
// ============================================================

#pragma once

#include <Arduino.h>

void setupHome();

// Lecture anti-rebond et comptage des detections. A appeler a chaque
// tour de loop().
void updateHome();

// true si du metal est devant le capteur.
bool homeDetecte();

// {"detecte":..,"niveau":0/1,"mV":..,"detections":..}
String homeJson();

// Remet le compteur de detections a zero.
void homeRemiseAZero();

// Derniere detection confirmee pas encore lue : renvoie true une seule
// fois par detection, avec la position du codeur (comptage de service)
// a l'instant ou le metal est arrive devant le capteur.
bool homeLireDetection(int32_t &pos);

// Oublie une detection en attente (avant de lancer une recherche).
void homeOublierDetection();
