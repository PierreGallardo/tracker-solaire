# Contexte — Tracker solaire 2 axes ESP32

Je te donne le contexte complet d'un projet Arduino/ESP32 déjà fonctionnel mais dont le
code (un seul gros fichier .ino généré au fil de plusieurs sessions) mériterait d'être
restructuré proprement — sans rien casser de ce qui marche. **Le fichier actuel est
joint à ce message : `tracker_web_control53.ino`.**

## Ce que j'attends de toi

Reconstruis-moi un programme complet et propre à partir de ce fichier existant, avec
les dépendances (bibliothèques) qui vont avec. Objectifs :
- Séparer proprement en plusieurs fichiers/onglets si pertinent (ex: un fichier par
  sous-système : moteurs, capteurs, vent, serveur web, page HTML) plutôt qu'un seul
  fichier de 800 lignes.
- Garder EXACTEMENT le même comportement fonctionnel (rien à retirer, rien à changer
  dans la logique métier) sauf si je te le demande explicitement.
- Conserver le pinout, les seuils, les constantes de calibration tels quels (voir plus
  bas) — ce sont des valeurs mesurées sur le matériel réel, pas des exemples.
- Ne fusionne PAS encore le codeur Baumer ni le mode Auto/Manuel — ces deux points sont
  listés dans "reste à faire" ci-dessous, on les traitera dans une étape séparée une
  fois la restructuration validée.

## Matériel

- ESP32 Dev Module sur carte d'extension breakout à borniers à vis (défaut de
  continuité sur certaines bornes GND constaté — toujours relier plusieurs bornes GND
  entre elles)
- 2× drivers moteur BTS7960/IBT_2 (l'un sérigraphié HW-039, même pinout)
- Codeur Baumer RS422 + 2× convertisseur MAX490 (pont diviseur de tension sur RXD avant
  entrée ESP32) — lecture CHA/CHB codée séparément, PAS ENCORE intégrée au programme
  principal
- Capteur HOME azimut
- Potentiomètre redondant 3B3 (4 fils, 2 curseurs) — axe inclinaison, mesure réelle
- BMP280 (clone HW-611, chip ID 0x58) — pression/température locale (PAS un BME280,
  donc pas de mesure d'humidité)
- RTC DS3231 — horloge temps réel, synchronisée une fois au démarrage via NTP
- Anémomètre HWFS-1 à contact sec (2 fils, pas de polarité, pas d'alimentation requise)
- Alimentation 12V, convertisseur 12V→5V séparé

## Pinout GPIO définitif

| Signal                        | GPIO |
|--------------------------------|------|
| RPWM azimut                    | 26   |
| LPWM azimut                    | 25   |
| RPWM inclinaison                | 27   |
| LPWM inclinaison                | 14   |
| Codeur Baumer CHA               | 34   |
| Codeur Baumer CHB               | 35   |
| Capteur HOME azimut             | 32   |
| Potentiomètre 3B3 canal A (SVP) | 36   |
| Potentiomètre 3B3 canal B (SVN) | 39   |
| I2C SDA (BMP280 + RTC)          | 18   |
| I2C SCL (BMP280 + RTC)          | 19   |
| Anémomètre                      | 4    |

## Règles d'alimentation critiques

- BMP280 et RTC uniquement sur le 3.3V de l'ESP32 (jamais un régulateur séparé, sinon
  instabilité I2C constatée)
- Drivers BTS7960 VCC/R_EN/L_EN → 5V du convertisseur séparé (jamais sur VIN de l'ESP32)
- Masse commune obligatoire entre ESP32, drivers, alimentation 12V

## Calibration potentiomètre 3B3

- 0° (à plat) : canal A = 3918 / canal B = 137
- 40° (déployé) : canal A = 269 / canal B = 3684

## Vent — anémomètre et logique de sécurité

- Facteur de conversion : 2.4 km/h par impulsion/seconde (constante à ajuster si la
  fiche technique exacte du HWFS-1 donne une autre valeur)
- Fenêtre de mesure de vitesse : 5 secondes, avec anti-rebond logiciel de 15 ms
- Seuil de sécurité (retour à plat automatique du tracker) : 50 km/h, avec hystérésis
  (désactivation sous 80% du seuil, soit 40 km/h)
- Tendance (augmentation/diminution) : buffer circulaire de 6 échantillons (= fenêtre
  glissante de 30s), comparaison moyenne 1ère moitié vs 2ème moitié, seuil de 2 km/h
  pour ignorer le bruit ; résultat "up"/"down"/"stable" exposé dans le JSON `/status`
- Voyants de l'interface web : vert 0-20 km/h, orange clignotant 20-40 km/h, rouge fixe
  au-delà de 40 km/h — seuils volontairement distincts et plus bas que le seuil de
  sécurité réel (50 km/h), pour avertir avant le déclenchement automatique

## Localisation

- Latitude : 47.1108 N / Longitude : 6.3032 E / Altitude : ~729m
- IP ESP32 sur le réseau local : 192.168.1.60, également accessible via
  http://tracker.local (mDNS)

## Ce qui fonctionne déjà (à ne pas casser)

- Pilotage des 2 ponts en H (RPWM/LPWM), validé au multimètre
- Calcul de position solaire (azimut/élévation) via algorithme NOAA + WiFi/NTP
- Asservissement de l'inclinaison sur le potentiomètre 3B3 (mesure réelle)
- Interface web complète : modèle 3D Three.js (chargé via CDN), commandes manuelles,
  météo Open-Meteo (régionale, en ligne), pression et température locales (BMP280),
  heure RTC, mesure du vent local (anémomètre) avec voyants + tendance, sécurité retour
  à plat automatique si vent ≥ 50 km/h

## Ce qui reste à faire (hors périmètre de cette restructuration, sauf demande explicite)

- Connecter physiquement les moteurs (le tracker est au jardin, le boîtier électronique
  est encore au bureau)
- Intégrer le codeur Baumer dans le programme principal (code de lecture CHA/CHB déjà
  écrit séparément, à fusionner)
- Remplacer l'azimut actuellement estimé (simulation basée sur le temps de commande)
  par la vraie mesure du codeur Baumer
- Compléter la station météo locale (direction du vent, pluviomètre)
- Ajouter un vrai mode "Suivi Solaire Auto" vs "Manuel" dans l'interface (actuellement
  seul le pilotage manuel + retour à plat sont exposés côté web)

## Bibliothèques Arduino requises

- Adafruit BMP280 Library + Adafruit Unified Sensor
- RTClib (Adafruit)
- ArduinoJson
- WiFi, WebServer, HTTPClient, ESPmDNS, Wire (incluses avec le core ESP32 Arduino)
- Three.js chargé via CDN dans la page HTML servie (pas une bibliothèque Arduino)

## Environnement

- Arduino IDE 2.2.1, carte cible "ESP32 Dev Module"
