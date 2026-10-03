// ============================================================
// Codeur.h - Lecture en quadrature du codeur Baumer (VERSION TEST)
// ------------------------------------------------------------
// Decode les voies CHA / CHB en x4 (chaque front compte) par
// interruption, et expose position, transitions par voie et
// erreurs de sequence.
//
// Un front qui "saute" un etat (A et B changent ensemble) est
// compte comme ERREUR : cela revele du bruit, un mauvais cablage
// ou une rotation trop rapide.
//
// Outil de diagnostic uniquement : non relie a l'azimut. L'ecoute
// est desactivee au demarrage et s'arrete seule apres
// CODEUR_DIAG_DUREE_MS ou en cas de bruit excessif (entree non
// reliee), pour ne jamais perturber le reste du programme.
// ============================================================

#pragma once

#include <Arduino.h>

// Active / coupe l'ecoute. L'activation remet les compteurs a zero.
void codeurActiver(bool actif);

// Remet position, transitions et erreurs a zero (sans couper l'ecoute).
void codeurRemiseAZero();

// Surveillance du taux de transitions et arret automatique.
// A appeler a chaque tour de loop().
void updateCodeur();

// Enregistreur : arme la capture des 128 prochaines transitions (A,B,
// delai en us). Lecture : codeurTraceJson().
void codeurTraceArmer();
String codeurTraceJson();

// {"actif":..,"position":..,"transA":..,"transB":..,"erreurs":..,
//  "niveauA":0/1,"niveauB":0/1,"bruit":..,"secRestantes":..}
String codeurJson();

// ============================================================
// COMPTAGE DE SERVICE (compteur materiel PCNT de l'ESP32)
// ------------------------------------------------------------
// Independant de l'outil de test ci-dessus : la quadrature x4 est
// decodee par le materiel, sans interruption a chaque front. Il tient
// donc la vitesse du codeur monte sur l'arbre moteur, bien au-dela
// de ce que l'outil de test peut suivre. Toujours actif.
// ============================================================

void setupCodeurService();
bool codeurServiceOK();
int32_t codeurServicePosition();
