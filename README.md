# Tracker solaire 2 axes

Firmware ESP32 d'un tracker solaire (inclinaison + azimut) et tout ce qu'il
faut pour l'utiliser depuis un serveur Debian : interface web accessible
depuis Internet (HTTPS + mot de passe), compilation et flashage du firmware
par WiFi.

```
                Internet                            Serveur Debian (Docker)                Réseau local
 ┌───────────┐   HTTPS   ┌─────────────────────┐  HTTP  ┌──────────────────────┐  HTTP  ┌────────┐
 │ Navigateur│ ────────► │ Nginx Proxy Manager │ ─────► │ tracker-web (Caddy)  │ ─────► │ ESP32  │
 └───────────┘           │ domaine, certificat │        │ mot de passe, pages  │        │tracker │
                         └─────────────────────┘        ├──────────────────────┤        │        │
                                                        │ tracker-build / ota  │ ─OTA─► │        │
                                                        └──────────────────────┘        └────────┘
```

| Dossier | Contenu |
|---|---|
| `tracker_web_control54/` | Le firmware (croquis Arduino). Voir son [README](tracker_web_control54/README.md) et [CABLAGE.md](tracker_web_control54/CABLAGE.md). |
| `tracker_docker/` | Les 3 services Docker du serveur (voir ci-dessous). |
| `tracker_docker/web/` | L'image de l'interface web. |

## Les 3 services Docker

| Service | Rôle | Quand |
|---|---|---|
| `tracker-web` | Vérifie le mot de passe, sert les pages, relaie les commandes à l'ESP32. Joignable seulement par Nginx Proxy Manager | Tourne en permanence |
| `tracker-build` | Compile le firmware (`arduino-cli`) | À la demande |
| `tracker-ota` | Envoie le firmware compilé à l'ESP32 par WiFi | À la demande |

**Qui fait quoi pour l'accès web ?** Nginx Proxy Manager (NPM) garde les
ports 80/443, le nom de domaine et le certificat HTTPS, comme pour les autres
services du serveur. `tracker-web` ne publie aucun port sur le réseau : il
est branché sur le réseau Docker de NPM, qui le joint par son nom. Le mot de
passe est vérifié par `tracker-web` lui-même, donc il protège le tracker même
si la configuration de NPM change.

**D'où viennent les pages web ?** Elles restent écrites dans le firmware
(`PageWeb.h`, `PageCalib.h`, `PageCodeur.h`), comme avant. À la construction
de l'image `tracker-web`, le script `extraire_pages.py` les en extrait à
l'identique (vérifié octet par octet). On modifie donc toujours les pages au
même endroit, et l'ESP32 continue de les servir lui-même sur le réseau local.

---

## 1. Prérequis

- Un serveur **Debian** avec Docker, le plugin Compose et **Nginx Proxy
  Manager** déjà en place (il occupe les ports 80 et 443).
- ⚠️ **Le serveur doit être sur le même réseau local que l'ESP32.** Il le
  contacte directement (`http://192.168.1.60`) et le flashage OTA ne
  fonctionne qu'en local. Si ton serveur est ailleurs (VPS, autre maison), il
  faudra d'abord relier les deux réseaux par un VPN (WireGuard, Tailscale…).
- **Une IP fixe pour l'ESP32** : réserve `192.168.1.60` à sa carte WiFi dans
  la box (bail DHCP statique), sinon l'adresse peut changer après une coupure.
- **Un nom de domaine** (ou sous-domaine, ex. `tracker.mon-domaine.fr`) qui
  pointe vers le serveur, comme pour tes autres services.

> **Ne redirige jamais un port de la box directement vers l'ESP32.** Ses
> commandes moteur n'ont aucun mot de passe : l'accès Internet doit
> toujours passer par NPM puis `tracker-web`.

## 2. Installation

```bash
git clone <URL_DU_DEPOT> tracker-solaire
cd tracker-solaire
```

**a) Identifiants du firmware** (WiFi + OTA) — fichier jamais publié :
```bash
cp tracker_web_control54/Secrets.exemple.h tracker_web_control54/Secrets.h
nano tracker_web_control54/Secrets.h        # WiFi, mot de passe OTA, position
```

**b) Configuration du serveur** :
```bash
cd tracker_docker
cp .env.example .env
nano .env
```
À remplir :
- `PROXY_NETWORK` : le nom du réseau Docker de NPM. Pour le trouver :
  ```bash
  docker ps --format '{{.Names}}' | grep -i proxy        # nom du conteneur NPM
  docker inspect <conteneur_npm> --format '{{range $k,$v := .NetworkSettings.Networks}}{{$k}} {{end}}'
  ```
  (souvent `npm_default`, `nginx-proxy-manager_default` ou `proxy`)
- `OTA_PASSWORD` : le même que dans `Secrets.h`.
- `HOST_IP` : l'adresse locale de ce serveur (`hostname -I`, première valeur).

Laisse `WEB_USER` et `WEB_PASSWORD_HASH` vides : c'est l'étape suivante.

**c) Identifiant et mot de passe de l'interface web** :
```bash
./definir-mot-de-passe.sh
```
Le script construit l'image la première fois (une minute), demande un
identifiant et un mot de passe (12 caractères minimum, la saisie ne
s'affiche pas), et n'enregistre dans `.env` que son **empreinte**.

**d) Démarrage et test depuis le serveur** :
```bash
docker compose up -d tracker-web
docker compose logs tracker-web             # doit afficher "Interface du tracker : ..."
curl -u <identifiant> http://127.0.0.1:8088/status
```
Le `curl` demande le mot de passe puis doit afficher le JSON de l'ESP32. Le
port `8088` n'est ouvert que sur le serveur lui-même (`127.0.0.1`), pas sur
le réseau.

**e) Hôte dans Nginx Proxy Manager** — **Hosts** → **Proxy Hosts** →
**Add Proxy Host** :

| Onglet | Réglage | Valeur |
|---|---|---|
| Details | Domain Names | `tracker.mon-domaine.fr` |
| Details | Scheme | `http` |
| Details | Forward Hostname / IP | `tracker-web` |
| Details | Forward Port | `80` |
| Details | Block Common Exploits | activé |
| Details | Cache Assets / Websockets Support | désactivés |
| Details | Access List | **Publicly Accessible** (voir la note) |
| SSL | SSL Certificate | Request a new SSL Certificate |
| SSL | Force SSL, HTTP/2 Support | activés |

> **Pas d'Access List dans NPM.** Le mot de passe est déjà demandé par
> `tracker-web`. Une Access List NPM utilise le même mécanisme (en-tête
> `Authorization`) et, par défaut, ne le transmet pas : le navigateur
> redemanderait le mot de passe en boucle.

Ouvre ensuite `https://tracker.mon-domaine.fr` : le navigateur demande
l'identifiant et le mot de passe, puis l'interface s'affiche.

Le service redémarre tout seul après un redémarrage du serveur
(`restart: unless-stopped`).

## 3. Utilisation au quotidien

| Je veux… | Commande (dans `tracker_docker/`) |
|---|---|
| Voir si l'interface tourne | `docker compose ps` |
| Voir les journaux | `docker compose logs --tail 50 tracker-web` |
| Changer le mot de passe web | `./definir-mot-de-passe.sh` puis `docker compose up -d tracker-web` |
| Appliquer une modif d'une page HTML | `docker compose build tracker-web && docker compose up -d tracker-web` |
| Compiler le firmware | `docker compose build tracker-build` (1 fois, long) puis `docker compose run --rm tracker-build` |
| Flasher l'ESP32 par WiFi | `docker compose --profile flash run --rm tracker-ota` |

Après une modification d'une page dans un fichier `Page*.h`, il faut
**à la fois** reconstruire `tracker-web` (pour l'accès via le serveur) **et**
reflasher l'ESP32 (pour l'accès direct en local) — sinon les deux versions
divergent.

## 4. La calibration se fait sur place

La calibration de l'azimut fait tourner le tracker : le firmware la **refuse
volontairement** pour toute requête passée par un proxy (décision de
sécurité de la v54, conservée). Depuis la maison, ouvre directement :

```
http://192.168.1.60/calibration
```

Via le serveur, la page `/calibration` s'affiche, mais ses commandes
répondent « Calibration refusée à distance » — c'est normal.

## 5. Sécurité : ce qui est protégé, et comment

- **Depuis Internet, on peut arrêter le tracker mais pas le déplacer**
  (il n'y a pas de caméra sur place). `tracker-web` répond `403` à
  `/monter`, `/descendre`, `/gauche`, `/droite`, `/suivi_auto` et aux
  commandes de calibration, sans les transmettre à l'ESP32. Restent
  autorisés : les arrêts, le retour à plat, le mode manuel et les
  acquittements. Pour déplacer le tracker, utiliser l'adresse locale
  `http://192.168.1.60` depuis la maison.
- **Une commande lancée depuis un autre site est refusée** (`403`) : le
  navigateur renvoie le mot de passe tout seul, un lien piégé ailleurs ne
  doit pas pouvoir arrêter ou remettre à plat le tracker.

- **Tout le site** (pages, données, commandes) exige identifiant + mot de
  passe, vérifiés par `tracker-web`. Sans eux, aucune requête n'atteint
  l'ESP32 (vérifié en test, y compris à travers Nginx).
- **HTTPS** assuré par Nginx Proxy Manager : le mot de passe ne circule
  jamais en clair sur Internet.
- **`tracker-web` ne publie aucun port sur le réseau** : seul NPM (réseau
  Docker partagé) et le serveur lui-même (`127.0.0.1:8088`) peuvent le joindre.
- **Le mot de passe web n'est stocké nulle part en clair** : seule son
  empreinte bcrypt est dans `.env`.
- **Démarrage refusé** si la configuration est incomplète ou si l'empreinte
  est abîmée (plutôt que de lancer un site mal protégé).
- **L'OTA n'est jamais exposé** : le port 3232 n'est pas publié et le
  flashage ne se fait que depuis le réseau local.
- **Rien de secret dans Git** : `Secrets.h`, `.env` et les binaires `.bin`
  (qui contiennent le mot de passe WiFi en clair) sont exclus par
  `.gitignore`, quel que soit leur dossier. L'image web ne peut lire que les
  3 pages HTML du firmware (liste blanche dans `.dockerignore`).

## 6. Dépannage

| Symptôme | Cause probable |
|---|---|
| `network proxy declared as external, but could not be found` | `PROXY_NETWORK` ne correspond pas au réseau de NPM → étape 2b |
| Le conteneur s'arrête avec « variable(s) manquante(s) » | `.env` incomplet → relire l'étape 2b / 2c |
| « n'est pas une empreinte bcrypt valide » | `WEB_PASSWORD_HASH` modifié à la main sans apostrophes → relancer `./definir-mot-de-passe.sh` |
| Page **502 Bad Gateway** de NPM | `tracker-web` arrêté, ou pas sur le réseau de NPM → `docker compose ps`, vérifier `PROXY_NETWORK` |
| Texte « Tracker injoignable » | `tracker-web` marche, mais l'ESP32 ne répond pas : éteint, hors WiFi, en cours de flashage, ou IP changée (voir le bail DHCP) |
| Le mot de passe est redemandé en boucle | Une Access List est activée dans NPM → la remettre sur « Publicly Accessible » |
| « Commande refusée à distance » | Normal : les mouvements sont interdits depuis Internet (voir §5). Passer par `http://192.168.1.60` à la maison |
| Modèle 3D absent, page figée | Le **navigateur** n'a pas accès à Internet : Three.js est chargé depuis un CDN (comportement d'origine du firmware) |

## 7. Bon à savoir

- Le petit serveur web de l'ESP32 traite une requête à la fois, et la page
  l'interroge chaque seconde : évite de laisser beaucoup d'onglets ouverts
  en même temps.
- Les commandes du firmware sont des requêtes `GET` (`/monter`, `/gauche`…),
  comme dans la version d'origine. Le mot de passe et le refus des
  commandes venant d'un autre site les protègent ; une évolution future
  pourrait les passer en `POST`.
- Versions testées : Caddy 2.10.2, Nginx 1.24 (en-têtes de NPM), core ESP32 3.3.11.
