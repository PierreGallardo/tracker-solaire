// ============================================================
// Vent.h - Anemometre HWFS-1 : vitesse, rafales, tendance,
//          paliers de securite et historique des evenements
// ------------------------------------------------------------
// Le HWFS-1 est un capteur ANALOGIQUE PASSIF : une generatrice
// interne delivre 0 a 3,3 V proportionnellement a la vitesse.
// Ni alimentation, ni pull-up, ni interruption - une simple
// entree ADC1 (l'ADC2 est inutilisable quand le WiFi tourne).
//
// - Echantillonnage de l'ADC toutes les PERIODE_ECHANT_MS
// - Vitesse moyenne : moyenne des echantillons sur 5 s
// - Vitesse de pic  : MAXIMUM des echantillons sur 2 s
// - Tendance sur un buffer circulaire glissant de 30 s
//
// Deux paliers, inspires des trackers professionnels :
//   1. SEUIL_VENT_REDUIT (40 km/h) -> mode reduit : amplitude
//      plafonnee a 20 deg et vitesse PWM abaissee a 120.
//   2. SEUIL_VENT_SECURITE (50 km/h moyenne sur 5 s) ou
//      SEUIL_RAFALE_SECURITE (50 km/h pic sur 2 s)
//      -> retour a plat automatique.
// Les deux paliers ont une hysteresis de sortie a 80 %.
// ============================================================

#pragma once

#include <Arduino.h>

// Ouvre le stockage NVS et recharge l'historique sauvegarde.
// A appeler dans setup(), apres setupCapteursI2C().
void initHistoriqueVentNVS();

// Configure la broche + l'interruption, et arme les deux fenetres.
void setupAnemometre();

// Recalcule vitesse moyenne / tendance / paliers de securite si la
// fenetre de 5 s est ecoulee. A appeler a chaque tour de loop().
void updateVent();

// Recalcule la vitesse de pic sur la fenetre courte de 2 s et
// declenche la securite en cas de rafale, meme si la moyenne sur
// 5 s reste basse. A appeler a chaque tour de loop().
void updateRafale();

// Derniere vitesse moyenne mesuree (fenetre 5 s), en km/h.
float ventVitesseKmh();

// Derniere vitesse de pic mesuree (fenetre 2 s), en km/h.
float ventRafaleKmh();

// true tant que le vent impose le retour a plat (hysteresis incluse).
bool ventSecuriteActive();

// true tant que le vent impose l'amplitude et la vitesse reduites.
bool ventModeReduit();

// Secondes restantes avant que la securite puisse etre levee
// (0 = securite inactive, ou delai pas encore demarre).
unsigned long ventSecondesAvantLevee();

// --- Surveillance du capteur ---------------------------------
// Detecte une panne silencieuse : zero impulsion se lit sinon
// comme "pas de vent", et desarme toute la securite.
// A appeler a chaque tour de loop().
void updateDiagnosticAnemo();

// L'alerte reste levee jusqu'a acquittement manuel par l'operateur.
void acquitterAnemo();
bool anemoEstSuspect();
bool anemoEstAcquitte();
const String& anemoCauseSuspicion();

// Repere de bon fonctionnement : la tension lue a l'instant.
// 0 V au repos, elle monte avec la vitesse du vent.
float anemoTensionV();
float anemoTensionMaxV();
unsigned long anemoSecondesDepuisActivite();

// --- Acquittement de l'alerte vent ---------------------------
// L'alerte reste levee apres une mise en securite jusqu'a ce que
// l'operateur l'acquitte, meme si la securite s'est levee entre-temps.
// Acquitter = "j'ai vu" : cela ne leve PAS la securite.
void acquitterVent();
bool        ventAlerteEnAttente();
float       ventAlerteVitesse();
const char* ventAlerteCause();
const char* ventAlerteHeure();

// --- Episode de vent fort (suivi rapproche) ------------------
// Des qu'un echantillon depasse SEUIL_EPISODE_KMH, un releve est fait
// toutes les 5 s et les NB_VALEURS_EPISODE dernieres valeurs sont
// conservees, avec une tendance hausse / baisse / stable.
// Purement informatif : aucun effet sur les moteurs.
bool          ventEpisodeActif();
const String& ventEpisodeTendance();
int           ventEpisodeNbValeurs();

// {"actif":true,"heure":"08:52:46","tendance":"up",
//  "valeurs":[{"p":43.9,"m":38.2}, ...]}  du plus ancien au plus recent.
String episodeVentJson();

// "up" (augmentation) / "down" (diminution) / "stable".
const String& ventTendance();

// Vitesse PWM a appliquer aux moteurs : VITESSE_REDUITE en mode
// reduit, VITESSE_MANUELLE sinon.
uint8_t ventVitesseMoteur();

// Serialise l'historique des mises en securite au format
// [{"h":"14:32:05","v":52.3,"c":"moyenne"}, ...]
// du plus ancien au plus recent. Renvoie "[]" si aucun evenement.
String historiqueVentJson();
