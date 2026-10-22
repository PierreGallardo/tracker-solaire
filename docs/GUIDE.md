# Guide du tracker solaire

Ce guide explique **comment utiliser et entretenir** le tracker au quotidien :
l'interface web, la gestion du vent, la calibration, la mise à jour du
programme et les points à surveiller.

Pour l'installation technique, voir les autres documents :

| Document | Contenu |
|---|---|
| [README.md](../README.md) | Installation sur le serveur (Docker, Nginx Proxy Manager, mot de passe), sécurité, dépannage du serveur |
| [tracker_web_control54/README.md](../tracker_web_control54/README.md) | Organisation du programme, bibliothèques, choix techniques |
| [tracker_web_control54/CABLAGE.md](../tracker_web_control54/CABLAGE.md) | Câblage de tous les capteurs |
| [tracker_web_control54/MODIFICATIONS_RAFALES.md](../tracker_web_control54/MODIFICATIONS_RAFALES.md) | Rafales répétées et bargraphe : détail et checklist de tests |

---

## 1. Vue d'ensemble

Le tracker oriente les panneaux sur **deux axes** :

- l'**inclinaison** (0 à 40°), mesurée par un potentiomètre 3B3 ;
- l'**azimut** (rotation gauche/droite), mesuré par un codeur Baumer une fois
  la calibration faite.

Tout est piloté par un **ESP32** qui sert lui-même son interface web. On peut
y accéder de deux façons :

| Accès | Adresse | Ce qu'on peut faire |
|---|---|---|
| **À la maison** (réseau local) | `http://192.168.1.60` ou `http://tracker.local` | Tout : surveiller, déplacer, calibrer |
| **Depuis Internet** | `https://<domaine du serveur>` (identifiant + mot de passe) | Surveiller et **arrêter** ; **pas de déplacement** |

Depuis Internet, le tracker ne peut pas être déplacé, parce qu'il n'y a pas de
caméra sur place : on ne verrait pas ce qu'on fait. On peut l'arrêter, le
remettre à plat, passer en mode manuel et acquitter les alertes.

---

## 2. L'interface web

La page se met à jour toute seule chaque seconde.

### Inclinaison

| Bouton | Effet |
|---|---|
| **☀ SUIVI AUTO** | L'inclinaison suit le soleil (`90° − élévation`, limitée à 0–40°). La nuit (soleil à moins de 5° au-dessus de l'horizon), le tracker se met à plat. |
| **✋ MANUEL** | Arrête le suivi automatique. |
| **▲ MONTER / ▼ DESCENDRE** | Déplacement manuel. Quitte le suivi automatique. |
| **■ STOP** | Arrête le moteur d'inclinaison. |
| **⟲ RETOUR A PLAT (0°)** | Remet les panneaux à plat. |

Par sécurité, **chaque mouvement manuel s'arrête tout seul après 15 secondes**.

### Azimut

**◀ GAUCHE**, **DROITE ▶** et **■ STOP**. L'azimut reste **toujours manuel** :
le suivi automatique ne pilote que l'inclinaison. Tant que la calibration
n'est pas validée, l'azimut affiché est une **estimation** (« AZIMUT ESTIME »)
qui peut dériver.

### Modèle 3D

Une vue 3D du tracker dans son décor reproduit sa position. On la fait
tourner en glissant, on zoome à la molette ou en pinçant. Le navigateur doit
avoir accès à Internet pour l'afficher.

### Météo et historiques

| Panneau | Contenu |
|---|---|
| **Météo (Open-Meteo, régionale)** | Température, vent et rafales de la région, risque de pluie. Mise à jour toutes les 10 min. |
| **Mises en sécurité (vent)** | Les 8 dernières mises en sécurité, avec l'heure, la vitesse et la cause. Conservées après un redémarrage. |
| **Enregistrement des rafales** | Bargraphe : une barre toutes les 15 s (vitesse maximale), sur 24 h. Zoom 24 h / 6 h / 1 h. |
| **Évolution locale** | Température et pression du capteur local, toutes les 30 min sur 12 h, avec tendance. Export CSV. |
| **Évolution du vent local** | Vent moyen et rafale maximale par demi-heure, sur 12 h. Export CSV. |
| **Relevés de l'anémomètre** | Une ligne par tranche de 10 min où l'anémomètre tourne : tension, vent calculé, vent régional, **constante suggérée**. Sert à vérifier le réglage de l'anémomètre. |

Les heures des graphiques sont en **UTC** (heure d'hiver − 1 h, heure d'été − 2 h).

---

## 3. Le vent : comment le tracker se protège

Toute la protection repose sur l'**anémomètre** du site. Plusieurs règles
s'empilent, de la plus douce à la plus forte :

| Règle | Se déclenche quand… | Effet | Se lève quand… |
|---|---|---|---|
| **Vent modéré** | vent moyen ≥ 40 km/h | Inclinaison bloquée à 20° max, moteurs ralentis | vent moyen < 32 km/h |
| **Rafales répétées** | 4 rafales au-dessus de 30 km/h en 3 min | Retour à plat | 15 min sans dépasser 20 km/h |
| **Mise en sécurité** | vent moyen sur 5 s ≥ 50 km/h, **ou** rafale sur 2 s ≥ 50 km/h | Retour à plat | **1 h complète** avec des rafales sous 30 km/h |

Points importants :

- La **mise en sécurité** est volontairement prudente : le moindre pic
  au-dessus de 30 km/h remet le compte d'une heure à zéro. Un coup de vent
  vient rarement seul.
- Les **rafales répétées** ne lèvent jamais une mise en sécurité en cours.
- En vent modéré, le tracker **ne redescend pas tout seul** à 20° : il refuse
  seulement de monter plus haut.
- Le **retour à plat n'a pas de limite de durée**.

### Les voyants et alertes

- **Voyants de vent** : vert sous 20 km/h, orange de 20 à 40, rouge au-delà.
  Ils préviennent avant que la sécurité ne se déclenche (50 km/h).
- **Épisode de vent** : au-delà de 30 km/h, un relevé toutes les 5 s
  s'affiche (15 dernières valeurs) avec la tendance. Il se termine après 30 s
  sous 30 km/h.
- **⚠ MISE EN SÉCURITÉ VENT** : reste affichée tant qu'on ne l'a pas
  **acquittée**, même si le vent est retombé entre-temps. Bouton
  **ACQUITTER** : on confirme l'avoir vue.
- **⚠ ANÉMOMÈTRE SUSPECT** : le capteur est peut-être en panne. Une panne ne se
  voit pas toute seule, car un anémomètre arrêté se lit comme « pas de vent ».
  L'alerte se déclenche si :
  - aucune tension n'est mesurée pendant **24 h** ;
  - ou le capteur reste à zéro pendant **30 min** alors que la météo
    régionale annonce **plus de 28 km/h**.

  Aller vérifier l'anémomètre (fils, coupelles qui tournent librement), puis
  **ACQUITTER**.

> ⚠️ **Toutes ces vitesses dépendent du réglage de l'anémomètre
> (50 km/h par volt), qui n'est pas encore validé.** Le tableau « Relevés de
> l'anémomètre » sert à le vérifier sur plusieurs jours de vent.

---

## 4. Calibration de l'azimut

La calibration permet d'afficher le **vrai azimut**, mesuré par le codeur, au
lieu d'une estimation. Elle **fait tourner le tracker** : elle se fait
**uniquement sur place**, à l'adresse locale :

```
http://192.168.1.60/calibration
```

Depuis Internet, la page s'affiche mais ses commandes sont refusées.

**Matériel** : un téléphone en mode boussole, réglé sur le **nord
géographique**, posé à plat, le haut tourné vers l'avant des panneaux.

| Étape | Ce qu'il faut faire |
|---|---|
| 1. Sens | Donner une courte impulsion avec GAUCHE puis STOP, regarder de quel côté les panneaux tournent, et indiquer quel bouton va vers l'**EST**. |
| 2. Point EST | Amener le tracker vers l'est, en arrivant **toujours dans le même sens**. Lire la boussole, entrer la valeur, relever. |
| 3. Point OUEST | Tourner vers l'ouest, au moins 30° plus loin (90° c'est mieux), puis relever. |
| 4. Contrôle (conseillé) | Placer le tracker entre les deux points et comparer : l'écart doit rester **sous 3°**. |
| 5. Recherche du HOME | Le tracker tourne seul et lentement vers l'est jusqu'au capteur HOME. **Rester à côté** : STOP l'arrête à tout moment. Arrêt automatique si le codeur ne compte pas, ou après 3 min. |
| 6. Butées | Amener le tracker à chaque extrémité **sûre pour les câbles** et l'enregistrer. Il ne pourra plus aller au-delà. |
| 7. Enregistrer | À partir de là, l'azimut affiché vient du codeur. |

> **Après une coupure de courant**, il faut relancer la **recherche du HOME**
> (étape 5) pour que l'azimut redevienne juste.

À ce jour, la calibration n'a été testée **que sans moteur**. La recherche du
HOME et les butées n'ont jamais été essayées avec les moteurs branchés.

---

## 5. Mettre à jour le programme

### Depuis le PC (IDE Arduino, par WiFi)

1. Ouvrir `tracker_web_control54/tracker_web_control54.ino` dans l'IDE Arduino.
2. Vérifier les réglages : carte **ESP32 Dev Module**, partition
   **Minimal SPIFFS** (`min_spiffs`).
3. Choisir le port réseau **tracker-solaire** (et non un port USB).
4. Téléverser ; le mot de passe OTA est demandé.
5. **Vérifier sur la carte** que la mise à jour a vraiment eu lieu : le temps
   de fonctionnement de l'ESP32 doit être revenu à zéro. L'outil peut annoncer
   un succès qui n'en est pas un.

Bon à savoir :

- Si l'envoi échoue, l'adresse du PC a peut-être changé (la box l'attribue
  automatiquement). Réserver les adresses de l'ESP32 et du PC dans la box
  évite ce problème.
- Une modification de `Config.h` oblige à **tout recompiler** : compter plus
  de 10 minutes.
- **Garder le câble USB accessible** : si une mise à jour casse le WiFi, seul
  l'USB permet de récupérer la carte.

### Depuis le serveur

Voir le [README](../README.md#3-utilisation-au-quotidien) : `tracker-build`
compile, `tracker-ota` envoie à la carte.

### Après avoir modifié une page web

Les pages (`PageWeb.h`, `PageCalib.h`, `PageCodeur.h`) sont servies **à la
fois** par l'ESP32 (accès local) et par le serveur (accès Internet). Après une
modification, il faut **les deux** :

- reflasher l'ESP32 ;
- reconstruire l'interface sur le serveur :
  ```bash
  docker compose up -d --build tracker-web
  ```

Sinon, la page vue depuis Internet ne sera plus la même que celle vue à la
maison.

---

## 6. Mots de passe et données personnelles

Les mots de passe (WiFi, OTA) et la position du site sont dans
`tracker_web_control54/Secrets.h`. **Ce fichier n'est jamais envoyé sur
GitHub.** Pour un nouveau poste, le recréer à partir de
`Secrets.exemple.h`.

Ne jamais partager :

- `Secrets.h` et `tracker_docker/.env` ;
- un **firmware compilé** (`.bin`) : il contient le mot de passe WiFi en clair.

Les mots de passe se transmettent séparément, jamais dans une archive ni dans
Git.

---

## 7. Travailler à deux sur le projet

Le code est sur GitHub : https://github.com/PierreGallardo/tracker-solaire

- La branche principale est **`dev`**.
- Chaque modification est faite sur une **branche séparée** puis proposée en
  **PR** (demande de fusion) vers `dev`.
- Pierre relit la PR, teste si besoin, puis la fusionne.

Ainsi, rien n'arrive dans `dev` sans avoir été relu.

---

## 8. Ce qui reste à faire

**Sur le tracker**
- [ ] Brancher les moteurs et le potentiomètre 3B3 (identifier les fils, contrôler les valeurs à plat).
- [ ] Monter le codeur et le HOME côté EST, puis faire la calibration sur place.
- [ ] Tester la recherche du HOME et les butées avec les moteurs.
- [ ] Faire la checklist de tests des rafales répétées et du bargraphe ([MODIFICATIONS_RAFALES.md](../tracker_web_control54/MODIFICATIONS_RAFALES.md)).
- [ ] Réserver les adresses de l'ESP32 et du PC dans la box.

**Anémomètre**
- [ ] Vérifier l'emplacement (hauteur, haies) et que les coupelles tournent librement.
- [ ] Valider le réglage de 50 km/h par volt grâce aux relevés, sur plusieurs jours de vent.
- [ ] Effacer les fausses rafales de l'historique (dues aux recâblages).

**Options proposées, non décidées**
- [ ] Limiter la durée du retour à plat.
- [ ] Redescendre réellement à 20° en vent modéré.
- [ ] Suivi automatique de l'azimut, une fois la calibration validée.
- [ ] Calculer la tendance du vent sur 2 h plutôt que sur 12 h.
- [ ] Corriger l'outil de diagnostic `/api/diag-analog`, qui modifie le réglage de la broche de l'anémomètre.

**Serveur**
- [ ] Configurer Nginx Proxy Manager et tester les images Docker sur le serveur.
