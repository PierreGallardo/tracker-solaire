# Tracker solaire 2 axes — ESP32 (v54)

Restructuration du sketch monolithique en modules. **Le comportement fonctionnel
est identique à la version précédente** : aucune logique métier n'a été modifiée,
aucun seuil ni aucune constante de calibration n'a été touché.

## Base de départ

Le fichier fourni était `tracker_web_control53.ino`. La version **v531**
(`../tracker_web_control531/`, enregistrée 3 minutes plus tard) contient en plus
la **tendance du vent** et les **voyants** décrits dans le cahier des charges —
c'est donc elle qui a servi de base, puisqu'elle correspond exactement au
comportement attendu. Les deux fichiers d'origine sont conservés tels quels dans
`reference/` (renommés en `.txt` pour que l'IDE Arduino ne tente pas de les
compiler avec le sketch).

## Organisation des fichiers

| Fichier | Rôle |
|---|---|
| `tracker_web_control54.ino` | `setup()` / `loop()` uniquement — l'enchaînement des sous-systèmes |
| `Config.h` | **Toutes** les constantes matérielles : pinout, seuils, calibration, WiFi. Le seul fichier à éditer quand le matériel change |
| `Moteurs.h/.cpp` | Pilotage des 2 ponts en H BTS7960 (RPWM/LPWM, 4 canaux PWM) |
| `Inclinaison.h/.cpp` | Mesure **réelle** de l'inclinaison via le potentiomètre 3B3 (2 curseurs, moyenne) |
| `Azimut.h/.cpp` | Position azimutale : **estimée** par le temps de commande, **mesurée** par le codeur une fois la calibration validée |
| `Vent.h/.cpp` | Anémomètre HWFS-1 (capteur **analogique** 0–3,3 V sur GPIO 32) : vitesse, rafale, tendance, sécurité, épisodes, historique NVS, diagnostic |
| `HistoMeteo.h/.cpp` | Historique température / pression toutes les 30 min (24 points, NVS, export CSV) |
| `HistoVent.h/.cpp` | Historique du vent : vent moyen et rafale max par demi-heure (24 points, NVS, export CSV) |
| `ReleveAnemo.h/.cpp` | Relevés de vérification de la constante vent : tension, vent local, vent régional, constante suggérée (NVS, CSV) |
| `Solaire.h/.cpp` | Position du soleil (NOAA) et inclinaison cible du suivi automatique |
| `Codeur.h/.cpp` | Codeur Baumer : outil de test par interruptions (`/codeur`) + comptage de service par le compteur matériel PCNT |
| `Home.h/.cpp` | Capteur HOME azimut SN04-N (GPIO 33) |
| `CalibAzimut.h/.cpp` | Calibration de l'azimut : 2 points boussole, HOME, butées logicielles, NVS |
| `PageCalib.h`, `PageCodeur.h` | Pages `/calibration` et `/codeur` |
| `CABLAGE.md` | Fiche de câblage de tous les capteurs |
| `CapteursI2C.h/.cpp` | BMP280 (clone HW-611) + RTC DS3231 + synchronisation NTP au démarrage |
| `Meteo.h/.cpp` | Météo régionale en ligne (Open-Meteo, sans clé API), vent et rafales comprises |
| `Controle.h/.cpp` | Machine à états du mouvement : commandes, arrêts automatiques, retour à plat, sécurité vent |
| `Reseau.h/.cpp` | Connexion WiFi + publication mDNS (`tracker.local`) |
| `ServeurWeb.h/.cpp` | Routes HTTP et `/status` JSON |
| `PageWeb.h` | La page HTML complète, stockée en flash (PROGMEM) |
| `reference/` | Les sketchs d'origine, conservés pour comparaison (non compilés) |

Les fichiers `.h`/`.cpp` apparaissent comme des onglets supplémentaires dans
l'IDE Arduino — il suffit d'ouvrir `tracker_web_control54.ino`.

## Bibliothèques requises

À installer via **Outils → Gérer les bibliothèques** :

- **Adafruit BMP280 Library** (tire *Adafruit BusIO*)
- **Adafruit Unified Sensor**
- **RTClib** (Adafruit)
- **ArduinoJson** (v7 — le code utilise `JsonDocument`, pas `StaticJsonDocument`)

Fournies avec le core ESP32 Arduino, rien à installer : `WiFi`, `WebServer`,
`HTTPClient`, `ESPmDNS`, `Wire`.

Three.js est chargé depuis un CDN par la page web servie — ce n'est pas une
bibliothèque Arduino, mais cela implique que **le navigateur qui affiche
l'interface doit avoir accès à Internet**.

Carte cible : **ESP32 Dev Module**. Testé avec le core `esp32:esp32` 3.3.11.

## Ce qui a changé (structure uniquement)

1. Un fichier de 800 lignes → 11 modules avec en-têtes commentés.
2. Toutes les constantes matérielles regroupées dans `Config.h`.
3. La page HTML n'est plus reconstruite à chaque requête par ~207
   concaténations de `String` : elle est stockée en flash et servie par
   `server.send_P()`. Le HTML envoyé est identique (extrait mécaniquement de la
   v531, seuls des retours à la ligne ont été ajoutés aux frontières des
   anciennes concaténations). Cela supprime l'allocation d'environ 24 ko de tas
   à chaque rafraîchissement de la page.
4. Les variables globales partagées sont devenues `static` dans leur module,
   avec des accesseurs — plus d'état global manipulable depuis n'importe où.
5. L'ordre des appels dans `loop()` est rigoureusement le même qu'avant.

Rien n'a été retiré, rien n'a été ajouté côté fonctionnalités.

## Gestion du vent — deux paliers

Inspiré des stratégies des trackers professionnels : plutôt que de passer
brutalement de « tout normal » à « retour à plat », on limite d'abord
l'amplitude et la vitesse des mouvements.

| Palier | Seuil | Sortie (hystérésis 80 %) | Effet |
|---|---|---|---|
| **Vent modéré** | 40 km/h moyenne | 32 km/h | inclinaison plafonnée à 20°, PWM 200 → 120 sur les 2 axes |
| **Sécurité** | 50 km/h moyenne (5 s) **ou** 50 km/h pic (2 s) | 40 km/h **et** 40 km/h | retour à plat automatique |

**Détection de rafale.** Une seconde fenêtre de mesure, courte (2 s), tourne en
parallèle de la moyenne sur 5 s, avec son propre compteur d'impulsions alimenté
par la même interruption. Un pic bref qui serait lissé par la moyenne déclenche
quand même la mise en sécurité.

**Historique.** Les 8 dernières mises en sécurité sont horodatées à la RTC
(`HH:MM:SS`, ou `??:??:??` si la RTC ne répond pas) avec la vitesse et la cause
(`moyenne` / `rafale`), exposées sur `GET /api/historique-vent` et affichées
sous les voyants (rafraîchies toutes les 15 s, masquées tant qu'il n'y a rien).
**En RAM uniquement : perdu à chaque redémarrage.**

## Suivi solaire automatique

Le calcul de position solaire NOAA vivait dans un sketch autonome
(`sketch_aug26e.ino`) qui ne faisait qu'afficher au moniteur série. Il est
maintenant intégré dans [Solaire.cpp](Solaire.cpp), avec une différence :
**la source de temps est la RTC DS3231 synchronisée NTP**, plus l'heure de
compilation. Comme la RTC est réglée en UTC, le calcul se fait avec un décalage
nul — aucun problème d'heure d'été à gérer.

Deux boutons dans l'interface : **☀ SUIVI AUTO** et **✋ MANUEL**. En auto,
l'inclinaison est asservie sur `90° − élévation solaire`, bornée à la course
mécanique du vérin (0–40°). La nuit (élévation < 5°), retour en position de
repos à plat.

Priorités : sécurité vent > mode vent réduit (plafond 20°) > suivi auto >
commandes manuelles. Toute commande d'inclinaison sort du mode auto ; les
commandes d'azimut ne le coupent pas, pour que tu puisses orienter l'azimut à la
main pendant que l'inclinaison suit le soleil.

**L'azimut n'est pas asservi.** Tant que le codeur Baumer n'est pas installé, la
position azimutale n'est qu'une estimation qui dérive : commander ce moteur en
aveugle risquerait la butée mécanique. À reprendre quand le codeur sera là.

## OTA — flashage par WiFi

Configuré dans [Reseau.cpp](Reseau.cpp), hôte `tracker-solaire`, port 3232, mot
de passe dans `Config.h`. `gererOTA()` est appelée en tout premier dans
`loop()`.

Le schéma de partition par défaut convient (`app0` + `app1`, 1,25 Mo chacune) —
vérifié dans le `partitions.csv` généré.

**Marge serrée : le croquis occupe 92 % de la partition.** Il reste environ
97 ko. Si un futur ajout fait dépasser 100 %, passer à
*Outils → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA)*.

**Garde le câble USB accessible sur le terrain** : si un code flashé casse le
WiFi ou le boot, l'OTA devient inaccessible et seul l'USB permet de récupérer.

## Persistance NVS de l'historique vent

L'historique survit maintenant aux redémarrages. Namespace `tracker`, clé
`histVent`, format `HH:MM:SS,vitesse,cause|` par événement. Sauvegarde
immédiate à chaque événement, rechargement au démarrage.

Avec 8 événements on est très loin des ~20 ko de la partition NVS. Pour en
garder beaucoup plus, il faudrait surveiller la fragmentation.

## Points à trancher / à surveiller

- **UNE SEULE DIVERGENCE avec `tracker_web_control53_with_OTA_NVS.ino`** —
  la condition de sortie de sécurité, dans `updateVent()` :

  ```cpp
  // ton fichier :
  if (vitesseVentKmh < SEUIL_VENT_SECURITE * 0.8) securiteVentActive = false;

  // ici :
  if (vitesseVentKmh < SEUIL_VENT_SECURITE * 0.8 &&
      vitesseRafaleKmh < SEUIL_RAFALE_SECURITE * 0.8) securiteVentActive = false;
  ```

  Avec la version de ton fichier, une rafale à 60 km/h arme la sécurité, puis la
  fenêtre de 5 s suivante la désarme aussitôt parce que la moyenne est restée à
  15 km/h. L'objectif annoncé de la détection de rafale — « ne pas laisser un pic
  bref être lissé par la moyenne » — n'est donc pas atteint. Le retour à plat
  déjà lancé va à son terme, mais l'état `securiteVent` s'éteint immédiatement
  côté web et la protection ne tient pas si le vent reste en rafales.
  Pour revenir exactement à ton comportement, supprimer les deux lignes de la
  condition sur `vitesseRafaleKmh` dans [Vent.cpp](Vent.cpp).
- **Le mode réduit ne fait pas redescendre le tracker.** Il refuse la commande
  MONTER au-delà de 20°, mais si le panneau est déjà à 35° quand le vent monte,
  il y reste jusqu'à ce que tu redescendes ou que le seuil de 50 km/h déclenche
  le retour à plat. Ta spec ne demandait qu'un refus de commande — dis-moi si tu
  veux un rappel automatique vers 20°.
- **Le retour à plat garde sa vitesse de 180.** La vitesse réduite (120) ne
  s'applique qu'aux 4 commandes manuelles, comme spécifié. Le retour à plat est
  une manœuvre de sécurité : le ralentir serait contre-productif.
- **`ventLocal` reste affiché uniquement si la météo en ligne répond.** Défaut
  hérité de la v531 : la vitesse du vent local est mise à jour dans le bloc
  `if(d.meteoValide)`, donc si Open-Meteo est injoignable, la valeur reste à
  `--` alors que le capteur fonctionne. Les voyants, le pic et le message de
  mode réduit, eux, sont bien hors de ce bloc. Non corrigé parce que ce serait
  un changement de comportement — à toi de dire.
- **Rafraîchissement météo après un échec.** Comportement hérité, conservé tel
  quel : si le tout premier `fetchMeteo()` échoue, `meteo.derniereMaj` reste à 0
  et l'ESP32 retente la requête à chaque tour de `loop()` au lieu d'attendre
  10 minutes. À corriger si tu veux, mais c'est un changement de comportement.
- Le seuil des voyants (vert < 20, orange 20–40, rouge > 40 km/h) est
  volontairement plus bas que le seuil de sécurité réel (50 km/h) : c'est un
  avertissement avant déclenchement, pas une incohérence.

## État au 9 octobre 2026 et reste à faire

Fait et vérifié sur la carte :
- Codeur Baumer : quadrature validée (0 erreur), 1800 impulsions/tour, 7200 pas/tour.
- Capteur HOME SN04-N sur GPIO 33 : 10 passages = 10 détections.
- Page `/calibration` : contrôles de saisie et refus à distance vérifiés, **sans moteur**.
- Historique du vent par demi-heure et relevés de l'anémomètre : en ligne depuis le 9 octobre 2026.

Reste à faire :
- Brancher les moteurs et le potentiomètre 3B3 sur le tracker.
- Vérifier le comptage matériel (PCNT) en tournant le codeur.
- Faire la calibration sur le tracker (recherche du HOME et butées jamais testées avec moteur).
- Valider le facteur de l'anémomètre (50 km/h par volt) contre la météo régionale : le tableau des relevés de l'anémomètre propose une valeur, à confirmer sur plusieurs jours de vent.
- Effacer les fausses rafales de l'historique vent (artefacts de câblage).
- Ajouter une limite de durée au retour à plat ; descendre à 20° en mode vent réduit (option).
- Suivi automatique de l'azimut, une fois la calibration validée.

## Rappels matériels critiques

- BMP280 et RTC **uniquement** sur le 3.3 V de l'ESP32 — jamais un régulateur
  séparé (instabilité I2C constatée).
- BTS7960 `VCC` / `R_EN` / `L_EN` → 5 V du convertisseur séparé, **jamais** sur
  le `VIN` de l'ESP32.
- Masse commune obligatoire entre ESP32, drivers et alimentation 12 V.
- Carte d'extension à borniers à vis : défaut de continuité constaté sur
  certaines bornes GND — relier plusieurs bornes GND entre elles.
