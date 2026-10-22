# Rafales répétées et bargraphe des rafales — livraison

> Dans le dépôt GitHub, le dossier `tracker_web_control54_rafales/` cité
> ci-dessous s'appelle `tracker_web_control54/` et son `.ino`
> `tracker_web_control54.ino` : le contenu est le même.

Date : 22 octobre 2026. **Rien n'a été téléversé.** Le projet en service
(`tracker_web_control54/`) n'a pas été modifié.

## 1. Le programme

Dossier `tracker_web_control54_rafales/`, fichier principal
`tracker_web_control54_rafales.ino` (l'IDE Arduino exige que le dossier porte le
nom du `.ino`). C'est une **copie de la v54 en service**, pas de la 53 : voir
l'ambiguïté n°1.

Compilation vérifiée (ESP32 Dev Module, core 3.3.11, partition `min_spiffs`,
avertissements activés) :

| Variante | Programme | Variables globales | Avertissements |
|---|---|---|---|
| Normale (`SIMULATION_VENT 0`) | 1 365 754 o (69 %) | 94 080 o (28 %) | aucun |
| Tests (`-DSIMULATION_VENT=1`) | 1 366 762 o (69 %) | 94 096 o (28 %) | le `#warning` volontaire de simulation |
| *Pour mémoire : v54 en service* | *1 314 734 o (66 %)* | *59 168 o (18 %)* | |

La logique des deux ajouts a été reproduite à l'identique en JavaScript et passée
au crible de 20 scénarios simulés échantillon par échantillon (hystérésis, fenêtre
glissante, relance du délai de 15 min, arrêt à 30 min, trous entre sessions,
tampon de 5760 barres, encodage JSON et décodage du navigateur) : **20/20**.
Ce n'est pas un test sur la carte : voir la checklist.

## 2. Résumé des modifications

Numéros de ligne = ceux de la copie.

### Fichiers nouveaux

| Fichier | Contenu |
|---|---|
| `EnregRafales.h` (47 l.) | interface du bargraphe |
| `EnregRafales.cpp` (269 l.) | tampon circulaire, session d'enregistrement, sauvegarde LittleFS par morceaux, rechargement, JSON par morceaux |

Fonctions de `EnregRafales.cpp` : `setupEnregRafales()` (monte LittleFS, recharge
le fichier, ou le fichier temporaire de secours), `enregRafalesEchantillon(kmh)`
(démarrage de session et maximum des 15 s), `updateEnregRafales()` (fermeture des
barres, arrêt après 30 min, sauvegarde), `enregRafalesActif()`,
`enregRafalesJson(depuis, ecrire)`.

### Fichiers modifiés

| Fichier | Lignes | Modification |
|---|---|---|
| `Config.h` | 197-246 | constantes AJOUT 1 (noms imposés), AJOUT 2, `SIMULATION_VENT` |
| `Vent.cpp` | 5 | `#include "EnregRafales.h"` |
| | 11-14 | constante `CAUSE_RAFALES_REPETEES` |
| | 106-109 | rechargement NVS : 3e cause « rafales repetees » reconnue |
| | 138-139 | tampon `alerteCause` agrandi de 9 à 17 octets |
| | 446-569 | **AJOUT 1** `rafalesDansFenetre()`, `traiterRafalesRepetees()` ; simulation (`#if SIMULATION_VENT`) |
| | 577-579 | vitesse simulée si `SIMULATION_VENT` |
| | 585-586 | appels par échantillon : rafales répétées + bargraphe |
| | 706-727 | accesseurs `ventMiseAPlatRequise()`, `ventMiseAPlatRafales()`, `ventNbRafalesFenetre()`, `ventSecondesAvantRedeploiement()` |
| `Vent.h` | 27, 113-135 | `#include "Config.h"` ; déclarations des accesseurs et de la simulation |
| `Controle.cpp` | 204-210 | `appliquerSecuriteVent()` lit `ventMiseAPlatRequise()` au lieu de `ventSecuriteActive()` ; libellé « RETOUR A PLAT (RAFALES) » |
| `CapteursI2C.h/.cpp` | 34-38 / 82-88 | lecture `lireEpochRTC()` (secondes UTC), sans changer le reste |
| `ServeurWeb.cpp` | 14 | include |
| | 92-97 | `/status` : `rafalesFenetre`, `miseAPlatRafales`, `secAvantRedeploiement`, `enregRafales` |
| | 199-226 | `/api/rafales?depuis=` (réponse par morceaux) ; `/api/sim-vent` (tests uniquement) |
| | 335-338 | déclaration des routes |
| `PageWeb.h` | 117-128 | styles du bargraphe |
| | 222-236 | bloc HTML (sous « Mises en sécurité (vent) ») |
| | 675-676, 685-688, 736-741 | affichage de la sécurité vent : cause « Rafales répétées », état « mise à plat (rafales répétées) » — voir ambiguïté n°7 |
| | 914-997 | `rafCouleur`, `rafHeure`, `rafZoom`, `rafraichirRafales`, `dessinerRafales` |
| | 1056-1057 | rafraîchissement toutes les 15 s, redessin au redimensionnement |
| `tracker_web_control54_rafales.ino` | 47, 88, 118 | include, `setupEnregRafales()`, `updateEnregRafales()` |

Rien n'a été modifié dans : moteurs, OTA, WiFi, RTC (hors lecture ajoutée), I2C,
pinout, paliers de vent existants, voyants, calibration, codeur, HOME.

## 3. Mémoire

### RAM

| Élément | Taille |
|---|---|
| Tampon des barres (5760 × 6 octets) | 34 560 o |
| Barres en attente pendant une sauvegarde (4 × 6) + en-tête + état | < 100 o |
| Horodatages des rafales (16 × 4) | 64 o |
| **Mesuré à la compilation** | **+34 912 o** (59 168 → 94 080) |
| Tampon de la réponse JSON (pile, pendant la requête) | 1 200 o |
| Reste pour la pile et le tas | 233 600 o |

### Flash

| Élément | Taille |
|---|---|
| Programme (code + bibliothèque LittleFS + page) | +51 Ko (66 % → 69 %) |
| Fichier `/rafales.bin` | 16 o d'en-tête + 6 o par barre = **34 576 o au maximum** |
| Pendant une sauvegarde (fichier temporaire + ancien) | ≈ 70 Ko au maximum, sur 128 Ko |

**Pourquoi LittleFS et pas NVS** : la partition NVS du schéma `min_spiffs` fait
20 Ko au total (5 pages de 4 Ko, dont une réservée au nettoyage), soit environ
15 Ko utiles, déjà partagés par les historiques vent, météo, vent 30 min,
relevés anémomètre et la calibration. 34,6 Ko n'y tiennent pas. La partition
`spiffs` (128 Ko) n'était pas utilisée : LittleFS la formate au premier démarrage.

**Usure** : une sauvegarde complète réécrit environ 10 blocs de 4 Ko. Au pire
(enregistrement 24 h/24, une sauvegarde toutes les 5 min) : environ 2 900
effacements par jour répartis sur 32 blocs, soit environ 90 cycles par bloc et
par jour. Pour une flash donnée à 100 000 cycles, environ 3 ans dans ce cas
extrême. En pratique, l'enregistrement ne tourne que les jours de vent et le
fichier est plus petit : plusieurs dizaines d'années.

## 4. Checklist de tests avant téléversement

Préparation : **moteurs débranchés** (toutes les sécurités réagissent au vent
simulé). Compiler avec `#define SIMULATION_VENT 1` dans `Config.h` (ou
`-DSIMULATION_VENT=1`), moniteur série à 115200 bauds.

**Démarrage**
- [ ] 1er démarrage : formatage LittleFS (1 à 2 s), pas de message « LittleFS indisponible ».
- [ ] `/status` contient `rafalesFenetre`, `miseAPlatRafales`, `secAvantRedeploiement`, `enregRafales`.
- [ ] La page s'affiche ; le bloc « Enregistrement des rafales » indique « arrêté ».

**AJOUT 1 — rafales répétées**
- [ ] `/api/sim-vent?scenario=rafales` : le moniteur affiche « Rafale > 30 km/h … 1 », puis 2, 3, 4, puis « RAFALES REPETEES : 4 en 3 min ».
- [ ] État vertical « RETOUR A PLAT (RAFALES) » ; bannière « Rafales répétées » ; ligne « Sécurité vent » en rouge ; nouvelle entrée « rafales repetees » dans l'historique.
- [ ] `secAvantRedeploiement` décompte depuis environ 900 (le scénario finit à 12 km/h).
- [ ] `/api/sim-vent?kmh=22` pendant le décompte : message « délai de redéploiement remis à zéro ».
- [ ] `/api/sim-vent?kmh=12` puis 15 min : « fin de la mise à plat » ; `miseAPlatRafales` repasse à `false`.
- [ ] Hystérésis : `kmh=32`, `kmh=28`, `kmh=32` → une seule rafale comptée ; `kmh=20` puis `kmh=32` → une deuxième.
- [ ] Priorité : pendant la mise à plat rafales, `kmh=55` → mise en sécurité existante. Puis `kmh=12` : à 15 min la règle rafales se lève, mais `securiteVent` reste `true` et le tracker reste à plat jusqu'à la fin de l'heure de calme existante.
- [ ] Redémarrage de la carte : l'entrée « rafales repetees » est toujours dans l'historique.

**AJOUT 2 — bargraphe**
- [ ] `kmh=10` : « enregistrement démarré » ; la page passe à « en cours depuis HH:MM UTC ».
- [ ] Une barre toutes les 15 s (`/api/rafales` : un segment qui s'allonge).
- [ ] `kmh=41` pendant 1 s, puis `kmh=10` : barre rouge à 41.
- [ ] `kmh=75` : barre violet foncé ; `kmh=25` : orange ; `kmh=12` : vert.
- [ ] `kmh=3` 29 min, `kmh=8` 1 s, `kmh=3` : l'enregistrement continue ; 30 min plus tard : « arrêté ».
- [ ] Nouvelle session après une pause : un trou apparaît sur l'axe des heures.
- [ ] Plus de 5 min après des barres : redémarrage (bouton EN) → « N barres rechargées », mêmes barres dans la page.
- [ ] `/api/rafales?depuis=<heure de la dernière barre>` ne renvoie que les nouvelles barres.
- [ ] Boutons 24 h / 6 h / 1 h ; affichage sur téléphone (portrait) ; pas d'erreur dans la console du navigateur.
- [ ] Pendant une sauvegarde (toutes les 5 min), la page et les commandes restent réactives.
- [ ] RTC débranchée : « Horloge RTC absente », aucune barre.

**Retour en service**
- [ ] Remettre `SIMULATION_VENT` à 0, recompiler : **aucun** `#warning` de simulation.
- [ ] Vérifier que `/api/sim-vent` répond 404.
- [ ] Moteurs rebranchés, puis téléversement.

## 5. Ambiguïtés et risques relevés (non corrigés)

1. **Programme de départ.** La demande cite `tracker_web_control53_with_OTA_NVS.ino` et un anémomètre sur **GPIO 33**. Le programme en service est la **v54 modulaire** : anémomètre sur **GPIO 32**, **GPIO 33 = capteur HOME**. Repartir de la 53 aurait supprimé codeur, HOME, calibration, historiques et la règle d'une heure de calme. J'ai travaillé sur une copie de la v54, pinout inchangé.
2. **Un seul fichier `.ino` demandé** : la v54 est découpée en modules, le livrable est donc un dossier de croquis.
3. **La gestion du vent existante diffère de la description.** En v54, la sortie de sécurité exige **1 h de calme ininterrompu** (rafale < 30 km/h), une règle demandée après la 53. Conservée telle quelle. Les voyants n'ont pas été vérifiés ni modifiés.
4. **Signal utilisé** : chaque échantillon de 100 ms, et non la moyenne 5 s ni le maximum 2 s, pour le comptage (30/25), le redéploiement (20) et le bargraphe (7). C'est le plus réactif, mais un seul échantillon de 100 ms suffit à relancer un délai.
5. **« Redéploiement automatique »** = fin de la demande de retour à plat. Le tracker ne remonte que si le suivi automatique est actif ; en mode manuel il reste à plat.
6. **Risque existant, non modifié** : une fois le retour à plat terminé pendant une mise en sécurité, le suivi automatique peut commander brièvement la montée avant que la sécurité ne redemande le retour à plat (quelques centaines de ms, de façon répétée).
7. **Affichage** : la demande ne prévoit l'état qu'en JSON. Sans adaptation, l'appli aurait affiché « Sécurité vent : OK » et « Vent moyen » alors que le tracker est à plat. J'ai modifié trois lignes d'affichage de la sécurité vent existante (`PageWeb.h` 675-676, 685-688, 736-741).
8. **Cause d'historique** : `alerteCause` agrandi (9 → 17 octets), sinon « rafales repetees » était tronqué. Elle s'affiche sans accent et sans couleur propre dans la liste.
9. **Rafales pendant la mise à plat** : le compteur n'est pas remis à zéro (non précisé). Sans effet pratique : 15 min de calme vident la fenêtre de 3 min.
10. **Barres sous 7 km/h** (dans les 30 min avant l'arrêt) : couleur non précisée, affichées en gris.
11. **#4B0082** sur fond sombre est peu contrasté ; la légende a un liseré clair, pas les barres.
12. **Heures en UTC**, comme le reste de l'appli (heure locale = UTC + 1 ou + 2).
13. **24 h ou 5760 barres** : le tampon est limité en nombre ; l'API et le graphe ne montrent que les 24 dernières heures. Avec des sessions espacées, des barres de plus de 24 h restent en mémoire et dans le fichier jusqu'à être poussées dehors.
14. **Heure des barres** : début de l'intervalle, calculée depuis le début de session (une seule lecture RTC par session). La RTC n'étant réglée par NTP qu'au démarrage, pas de saut en cours de session.
15. **Sans RTC**, pas d'enregistrement (impossible de placer les barres).
16. **Durée d'une sauvegarde non mesurée** : écrite par morceaux de 480 octets par tour de boucle, mais l'effacement d'un bloc de flash peut ponctuellement bloquer quelques dizaines de ms.
17. **Coupure de courant** : jusqu'à 5 min de barres perdues (sauvegarde toutes les 5 min, voulu). L'ancien fichier reste intact pendant l'écriture.
18. **Premier chargement de la page** : jusqu'à environ 23 Ko de données (24 h pleines), puis seulement les nouvelles barres.
19. **Simulation** : toutes les sécurités réagissent, moteurs à débrancher. `/api/sim-vent` n'existe que dans la variante de test ; ne jamais exposer une telle variante par le reverse proxy.
20. **Capteur** : l'anémomètre tourne actuellement très peu (site abrité ou frottement), et le facteur 50 km/h par volt n'est pas validé. Les seuils de 7, 20, 25 et 30 km/h dépendent de ce facteur.

## Aperçus

`apercu_bargraphe_rafales_24h.png` et `apercu_bargraphe_rafales_1h.png`, dans le
dossier parent : rendus avec le vrai code de la page et des données fictives.
