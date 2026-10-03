// ============================================================
// Controle.h - Etat et logique de mouvement du tracker
// ------------------------------------------------------------
// Centralise ce qui etait dispersé entre les handlers web et
// la loop() : commandes manuelles, arret automatique de
// securite, asservissement du retour a plat, securite vent.
// ============================================================

#pragma once

#include <Arduino.h>

// --- Etat expose a l'interface web -------------------------
extern String etatVertical;    // "STOP", "MONTER", "RETOUR A PLAT", ...
extern String etatHorizontal;  // "STOP", "GAUCHE", "DROITE"

// Secondes restantes avant l'arret automatique (0 = pas d'arret arme).
long secondesRestantesVertical();
long secondesRestantesHorizontal();

// --- Commandes manuelles (appelees par les routes HTTP) -----
void commandeMonter();
void commandeDescendre();
void commandeStopVertical();
void commandeRetourAPlat();

void commandeGauche();
void commandeDroite();
void commandeStopHorizontal();

// --- Mode "Suivi Solaire Auto" ------------------------------
// Asservit l'inclinaison sur l'elevation du soleil. Toute
// commande manuelle le desactive. La securite vent le suspend
// (retour a plat prioritaire) puis il reprend seul.
// L'azimut reste manuel tant que le codeur Baumer est absent.
void activerSuiviAuto();
void desactiverSuiviAuto();
bool suiviAutoActif();

// Inclinaison visee par le suivi auto, plafond vent applique.
double cibleSuiviAutoDeg();

// --- A appeler depuis loop() --------------------------------

// Arrets automatiques de securite + asservissement du retour a plat.
void updateControle();

// Declenche le retour a plat si l'anemometre est en alerte.
void appliquerSecuriteVent();
