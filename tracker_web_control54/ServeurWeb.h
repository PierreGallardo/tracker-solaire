// ============================================================
// ServeurWeb.h - Serveur HTTP + API JSON du tracker
// ------------------------------------------------------------
// Routes exposees :
//   GET /                  page de controle (voir PageWeb.h)
//   GET /status            etat complet, JSON, lu 1x/seconde
//   GET /monter            commandes manuelles axe inclinaison
//   GET /descendre
//   GET /stop_vertical
//   GET /retour_a_plat
//   GET /gauche            commandes manuelles axe azimut
//   GET /droite
//   GET /stop_horizontal
//
// Les commandes repondent par une redirection 303 vers "/",
// comme dans les versions precedentes.
// ============================================================

#pragma once

#include <Arduino.h>

// Enregistre les routes et demarre le serveur sur le port 80.
void setupServeurWeb();

// Traite les requetes en attente. A appeler a chaque tour de loop().
void serveurWebLoop();
