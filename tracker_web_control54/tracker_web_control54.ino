// ============================================================================
// TRACKER SOLAIRE 2 AXES - ESP32
// ----------------------------------------------------------------------------
// Version 54 : restructuration du sketch monolithique v531 en modules.
// Comportement fonctionnel identique a la v531 - aucune logique metier
// n'a ete modifiee, seule l'organisation du code change.
//
// Organisation des onglets :
//   Config.h        toutes les constantes materielles (pinout, seuils,
//                   calibration) - le seul fichier a editer pour le materiel
//   Moteurs         pilotage des 2 ponts en H BTS7960 (RPWM / LPWM)
//   Inclinaison     mesure REELLE via le potentiometre redondant 3B3
//   Azimut          position ESTIMEE (integration du temps de commande)
//   Vent            anemometre HWFS-1 : vitesse, tendance, securite
//   CapteursI2C     BMP280 (clone HW-611) + RTC DS3231 + synchro NTP
//   Meteo           meteo regionale en ligne (Open-Meteo)
//   Controle        machine a etats du mouvement (commandes, arrets auto,
//                   retour a plat, securite vent)
//   Reseau          WiFi + mDNS
//   ServeurWeb      routes HTTP + /status JSON
//   PageWeb.h       page HTML servie depuis la flash (PROGMEM)
//
// Materiel : ESP32 Dev Module, 2x BTS7960/IBT_2, potentiometre 3B3,
//            BMP280 HW-611, RTC DS3231, anemometre HWFS-1.
// Reseau   : 192.168.1.60 ou http://tracker.local
// Site     : 47.1108 N / 6.3032 E / ~729 m
//
// Bibliotheques requises (Gestionnaire de bibliotheques) :
//   Adafruit BMP280 Library, Adafruit Unified Sensor, RTClib, ArduinoJson
//   (WiFi, WebServer, HTTPClient, ESPmDNS, Wire viennent avec le core ESP32)
//
// PAS ENCORE INTEGRE : codeur Baumer RS422 (CHA 34 / CHB 35), capteur HOME
// azimut (32), mode "Suivi Solaire Auto", direction du vent, pluviometre.
// ============================================================================

#include "Config.h"
#include "Moteurs.h"
#include "Inclinaison.h"
#include "Azimut.h"
#include "Vent.h"
#include "CapteursI2C.h"
#include "Meteo.h"
#include "Solaire.h"
#include "HistoMeteo.h"
#include "Codeur.h"
#include "Home.h"
#include "CalibAzimut.h"
#include "Controle.h"
#include "Reseau.h"
#include "ServeurWeb.h"

#include <WiFi.h>

// Trace periodique sur le port serie, pour verifier que la carte tourne.
static void heartbeat() {
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();
    Serial.printf("[vivant] uptime: %lus | IP: %s\n",
                  millis() / 1000, WiFi.localIP().toString().c_str());
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  setupMoteurs();
  setupAzimut();
  setupAnemometre();
  setupHome();               // capteur HOME azimut (lecture de test)
  setupCodeurService();      // comptage materiel du codeur (toujours actif)
  setupCalibAzimut();        // calibration azimut enregistree (NVS)
  setupCapteursI2C();

  // NVS : recharge l'historique vent survivant au dernier redemarrage.
  initHistoriqueVentNVS();
  setupHistoMeteo();          // premier releve temperature/pression

  // Sans WiFi, pas de serveur web ni de meteo : on s'arrete ici.
  if (!connectWiFi()) return;

  synchroniserRTCDepuisNTP();
  demarrerMDNS();
  fetchMeteo();
  setupOTA();

  setupServeurWeb();

  Serial.print("Serveur Web démarré ! Ouvre : http://");
  Serial.println(WiFi.localIP());
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  gererOTA();                // demandes de flashage WiFi, en priorite
  serveurWebLoop();          // requetes HTTP en attente
  updateEstimationAzimut();  // integration de la position azimutale

  updateControle();          // arrets auto + asservissement retour a plat
  updateMeteo();             // rafraichissement meteo toutes les 10 min
  updateHistoMeteo();        // releve local toutes les 30 min

  updateVent();              // moyenne 5 s : vitesse, tendance, paliers
  updateRafale();            // pic 2 s : rafale breve non lissee
  updateDiagnosticAnemo();   // panne silencieuse du capteur
  updateCodeur();            // outil de test du codeur (inactif par defaut)
  updateHome();              // capteur HOME azimut (lecture de test)
  updateCalibAzimut();       // recherche du HOME + butees logicielles
  appliquerSecuriteVent();   // retour a plat si moyenne ou rafale >= 50 km/h

  heartbeat();
}
