# Fiche de câblage — tracker solaire ESP32

Chaque information est marquée :
- **[documenté]** : écrit dans le projet (pinout, calibration, règles)
- **[déduit]** : conclu des mesures ou de la structure du montage, à vérifier
- **[inconnu]** : rien dans le projet, à relever sur le matériel

## Pinout actuel

| Signal | GPIO | Remarque |
|---|---|---|
| RPWM / LPWM azimut | 26 / 25 | BTS7960 |
| RPWM / LPWM inclinaison | 27 / 14 | BTS7960 |
| I2C SDA / SCL | 18 / 19 | BMP280 + RTC, **3,3 V ESP32 uniquement** |
| Potentiomètre 3B3 canal A | 36 | borne « SVP » de la carte |
| Potentiomètre 3B3 canal B | 39 | borne « SVN » de la carte |
| Codeur Baumer CHA | 34 | via MAX490 + pont diviseur |
| Codeur Baumer CHB | 35 | via MAX490 + pont diviseur |
| **Anémomètre HWFS-1** | **32** | entrée analogique, voir plus bas |
| Capteur HOME azimut | **33** | SN04-N inductif NPN, via diode Schottky, voir plus bas |

« SVP » et « SVN » sont les noms de **broches de l'ESP32** (SENSOR_VP = GPIO 36,
SENSOR_VN = GPIO 39), sérigraphiés sur la carte d'extension. Ce ne sont **pas**
des noms de fils du potentiomètre.

## Potentiomètre 3B3 (inclinaison) — 4 fils, 2 curseurs

**[documenté]** 4 fils, 2 curseurs. Canal A → GPIO 36, canal B → GPIO 39.
Calibration : 0° → A = 3918, B = 137 ; 40° → A = 269, B = 3684.

**[déduit]** Les deux curseurs sont **complémentaires** : A + B vaut environ 4055
à 0° et 3953 à 40°, soit à peu près constant. Quand l'un monte, l'autre descend.

**[déduit]** Alimentation en **3,3 V** : les valeurs vont de 137 à 3918 sur 4095,
presque toute la plage de l'ADC. Avec du 5 V, les curseurs sortiraient bien
au-dessus de 3,3 V.

```
ESP32 3V3 ──────── fil « alim + » du 3B3
ESP32 GND ──────── fil « masse » du 3B3
GPIO 36 (SVP) ──── curseur A
GPIO 39 (SVN) ──── curseur B
```

**[inconnu]** La couleur de chaque fil.

### Identifier les fils avec un multimètre (3B3 débranché de l'ESP32)

1. Mode ohmmètre, tester les 6 paires de fils en bougeant l'axe.
2. La paire dont la résistance **ne change pas** = les deux extrémités de la
   piste (alim + et masse).
3. Les deux autres fils = les curseurs, dont la résistance vers les extrémités varie.

### Vérifications avant de brancher sur l'ESP32

- Alimenter le 3B3 en 3,3 V et mesurer la tension de chaque curseur : elle doit
  **toujours rester ≤ 3,3 V**.
- Une fois branché, à plat (0°), le brut doit valoir **A ≈ 3900 et B ≈ 140**. Si c'est
  inversé, permuter les deux fils signal : la calibration du code est faite pour A = haut à plat.

## Codeur Baumer RS422 — 2 voies, via 2× MAX490

**[documenté]** Chaîne : codeur RS422 → un MAX490 par voie → pont diviseur
sur la sortie du MAX490 (RXD) → GPIO 34 (CHA) et GPIO 35 (CHB).

```
Codeur A  ──── A  ┐
Codeur /A ──── B  ├─ MAX490 n°1 ── RXD ──[1 kΩ]──┬── GPIO 34
                  ┘ (VCC = 5 V)                  [2 kΩ]
                                                   │
                                                  GND
(idem pour B et /B → MAX490 n°2 → GPIO 35)
```

- Un MAX490 est alimenté en **5 V** (convertisseur séparé) et sa sortie sort
  donc à 5 V. Le pont diviseur est **obligatoire** : GPIO 34/35 ne supportent pas 5 V.
- Valeurs usuelles du pont : 1 kΩ en série + 2 kΩ vers la masse, soit
  5 × 2/3 ≈ 3,3 V. **[inconnu]** : les valeurs réellement montées.
- Côté MAX490, le récepteur utilise les bornes **A** et **B** ; les bornes Y et Z
  sont la sortie d'émission et ne servent pas ici.
- Permuter A et /A inverse le sens de comptage de la voie.
- **Masse commune** entre codeur, MAX490, ESP32 et alimentation.

**[documenté par l utilisateur]** Résolution : **1800 impulsions par tour** et par voie, soit
**7200 pas par tour** en décodage x4 (0,05° par pas). Essai du 2026-10-02 : quadrature
correcte (4 états, 0 erreur sur plus de 17 000 transitions) une fois le câblage corrigé.

**[inconnu]** À relever sur le matériel :
- la **référence** sur l'étiquette du codeur ;
- la **couleur** de chaque fil : A, /A, B, /B, éventuellement 0 et /0, alimentation, masse ;
- sa **tension d'alimentation**, à lire sur l'étiquette avant tout branchement (selon la
  version, 5 V ou une plage plus large) ;
- les valeurs des résistances du pont diviseur.

## Capteur HOME azimut — SN04-N (inductif, NPN, normalement ouvert)

**[documenté par l'utilisateur]** Gebildet SN04-N, 3 fils. Alimentation 10–30 V DC
(étiquette à vérifier), portée environ 4 mm sur l'acier, moins sur l'aluminium.
Couleurs standard : **marron = +**, **bleu = 0 V**, **noir = sortie**.

```
 +12 V ─────────────────── marron  ┐
                                   │  SN04-N
 GND commun ───────────┬── bleu    │  (LED rouge = métal détecté)
                       │           │
                       │   noir ───┘
                       │     │
                       │   ──┴──  diode Schottky (1N5819, BAT85…)
                       │    ▲     bague (cathode) côté fil NOIR
                       │    │
 3,3 V ESP32 ─[10 kΩ]──┼────┴──────────── GPIO 33
                       │                     │
                       └──────[100 nF]───────┘   (facultatif, anti-parasites)
```

- Pas de métal : la diode est bloquée, GPIO 33 est tiré à 3,3 V → **HAUT**.
- Métal devant : la sortie passe à 0 V, la diode conduit, GPIO 33 descend vers 0,3 V → **BAS**.
- La diode protège l'ESP32 : certains SN04-N ont une résistance de tirage interne vers
  le +12 V, et le fil noir sortirait alors en 12 V. **Ne jamais brancher le fil noir
  directement sur GPIO 33 sans avoir vérifié.**
- Une diode Schottky est nécessaire : une 1N4148 ou 1N4007 fait chuter 0,6 à 0,7 V,
  trop près du seuil de l'état bas de l'ESP32 (environ 0,8 V).
- **Masse commune** entre le 0 V de l'alimentation 12 V et le GND de l'ESP32.
- Test : `http://192.168.1.60/api/home` donne `detecte`, la tension réelle en `mV`
  (doit être inférieure à 600 mV en détection) et le nombre de détections
  (`?zero=1` le remet à zéro).

## Anémomètre HWFS-1 (état actuel)

- Capteur **passif analogique**, pas un contact sec. Sortie 0–3,3 V, aucune alimentation.
- Fils permutés **par rapport au câblage d'origine** : le sens de rotation naturel
  donne la bonne polarité.
  - fil **noir** → GPIO 32
  - fil **noir rayé** → GND
- **Recommandé** : une résistance de 1 kΩ en série sur le fil allant à GPIO 32, pour limiter
  le courant si une tension inverse arrivait sur la broche.
- ADC1 uniquement (32 ou 33). **Jamais** GPIO 4 : l'ADC2 est inutilisable avec le WiFi.

## Règles générales

- BMP280 et RTC : 3,3 V de l'ESP32 uniquement.
- BTS7960 : VCC / R_EN / L_EN sur le 5 V du convertisseur séparé, jamais sur VIN de l'ESP32.
- Masse commune obligatoire ESP32 / drivers / 12 V.
- Carte d'extension à borniers : défaut de continuité constaté sur certaines bornes GND.
  Toujours relier plusieurs bornes GND entre elles.
- GPIO 34, 35, 36, 39 : entrées seules, sans pull-up interne.
