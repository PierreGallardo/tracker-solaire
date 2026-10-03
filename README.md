# Tracker solaire 2 axes

Firmware ESP32 d'un tracker solaire (inclinaison + azimut) et tout ce qu'il
faut pour l'utiliser depuis un serveur Debian : interface web accessible
depuis Internet (HTTPS + mot de passe), compilation et flashage du firmware
par WiFi.

```
                          Internet                          Réseau local (maison)
 ┌───────────┐  HTTPS + mot de passe  ┌──────────────────────┐   HTTP    ┌────────┐
 │ Navigateur│ ─────────────────────► │ Serveur Debian       │ ────────► │ ESP32  │
 └───────────┘                        │  tracker-web (Caddy) │           │tracker │
                                      │  tracker-build / ota │ ──OTA───► │        │
                                      └──────────────────────┘           └────────┘
```

| Dossier | Contenu |
|---|---|
| `tracker_web_control54/` | Le firmware (croquis Arduino). Voir son [README](tracker_web_control54/README.md) et [CABLAGE.md](tracker_web_control54/CABLAGE.md). |
| `tracker_docker/` | Les 3 services Docker du serveur (voir ci-dessous). |
| `tracker_docker/web/` | L'image de l'interface web. |

## Les 3 services Docker

| Service | Rôle | Quand |
|---|---|---|
| `tracker-web` | Sert l'interface en HTTPS, protégée par mot de passe, et relaie les commandes à l'ESP32 | Tourne en permanence |
| `tracker-build` | Compile le firmware (`arduino-cli`) | À la demande |
| `tracker-ota` | Envoie le firmware compilé à l'ESP32 par WiFi | À la demande |

**D'où viennent les pages web ?** Elles restent écrites dans le firmware
(`PageWeb.h`, `PageCalib.h`, `PageCodeur.h`), comme avant. À la construction
de l'image `tracker-web`, le script `extraire_pages.py` les en extrait à
l'identique (vérifié octet par octet). On modifie donc toujours les pages au
même endroit, et l'ESP32 continue de les servir lui-même sur le réseau local.

---

## 1. Prérequis

- Un serveur **Debian** avec Docker et le plugin Compose :
  ```bash
  curl -fsSL https://get.docker.com | sudo sh
  sudo usermod -aG docker $USER     # puis se déconnecter / reconnecter
  docker compose version            # doit répondre
  ```
- ⚠️ **Le serveur doit être sur le même réseau local que l'ESP32.** Il le
  contacte directement (`http://192.168.1.60`) et le flashage OTA ne
  fonctionne qu'en local. Si ton serveur est ailleurs (VPS, autre maison), il
  faudra d'abord relier les deux réseaux par un VPN (WireGuard, Tailscale…).
- **Une IP fixe pour l'ESP32** : réserve `192.168.1.60` à sa carte WiFi dans
  la box (bail DHCP statique), sinon l'adresse peut changer après une coupure.
- Pour l'accès depuis Internet :
  - un **nom de domaine** (ou sous-domaine, ex. `tracker.mon-domaine.fr`)
    dont l'enregistrement DNS pointe vers l'IP publique de la box ;
  - les ports **80 et 443** de la box redirigés vers le serveur (80 sert
    uniquement à obtenir le certificat HTTPS, tout est ensuite redirigé
    vers 443).

> **Ne redirige jamais un port de la box directement vers l'ESP32.** Ses
> commandes moteur n'ont aucun mot de passe : l'accès Internet doit
> toujours passer par `tracker-web`.

## 2. Installation

```bash
git clone <URL_DU_DEPOT> tracker-solaire
cd tracker-solaire
```

**a) Identifiants du firmware** (WiFi + OTA) — fichier jamais publié :
```bash
cp tracker_web_control54/Secrets.h.example tracker_web_control54/Secrets.h
nano tracker_web_control54/Secrets.h        # remplir les 3 valeurs
```

**b) Configuration du serveur** :
```bash
cd tracker_docker
cp .env.example .env
nano .env
```
À remplir : `SITE_ADDRESS` (ton domaine), `OTA_PASSWORD` (le même que dans
`Secrets.h`), `HOST_IP` (l'adresse locale de ce serveur, ex. `192.168.1.77`).
Laisse `WEB_USER` et `WEB_PASSWORD_HASH` vides : c'est l'étape suivante.

**c) Identifiant et mot de passe de l'interface web** :
```bash
./definir-mot-de-passe.sh
```
Le script construit l'image la première fois (une minute), demande un
identifiant et un mot de passe (12 caractères minimum, la saisie ne
s'affiche pas), et n'enregistre dans `.env` que son **empreinte**.

**d) Démarrage** :
```bash
docker compose up -d tracker-web
docker compose logs -f tracker-web          # Ctrl+C pour quitter
```
Au premier démarrage, Caddy obtient le certificat HTTPS (quelques secondes).
Ouvre ensuite `https://tracker.mon-domaine.fr` : le navigateur demande
l'identifiant et le mot de passe.

Le service redémarre tout seul après un redémarrage du serveur
(`restart: unless-stopped`).

> **Tester d'abord en local, sans domaine** : mets `SITE_ADDRESS=:80` dans
> `.env`, relance `docker compose up -d tracker-web`, puis ouvre
> `http://<IP_DU_SERVEUR>` depuis la maison. Pas de HTTPS dans ce mode : ne
> pas l'ouvrir sur Internet ainsi.

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
volontairement** pour toute requête passée par le serveur (décision de
sécurité de la v54, conservée). Depuis la maison, ouvre directement :

```
http://192.168.1.60/calibration
```

Via le serveur, la page `/calibration` s'affiche, mais ses commandes
répondent « Calibration refusée à distance » — c'est normal.

## 5. Sécurité : ce qui est protégé, et comment

- **Tout le site** (pages, données, commandes) exige identifiant + mot de
  passe. Sans eux, aucune requête n'atteint l'ESP32 (vérifié en test).
- **HTTPS automatique** (Let's Encrypt, renouvelé tout seul) : le mot de
  passe ne circule jamais en clair sur Internet.
- **Le mot de passe web n'est stocké nulle part en clair** : seule son
  empreinte bcrypt est dans `.env`.
- **Démarrage refusé** si la configuration est incomplète ou si l'empreinte
  est abîmée (plutôt que de lancer un site mal protégé).
- **L'OTA n'est jamais exposé** : le port 3232 n'est pas publié et le
  flashage ne se fait que depuis le réseau local.
- **Rien de secret dans Git** : `Secrets.h`, `.env` et les binaires `.bin`
  (qui contiennent le mot de passe WiFi en clair) sont exclus par
  `.gitignore`. L'image web ne peut lire que les 3 pages HTML du firmware
  (liste blanche dans `.dockerignore`).

## 6. Dépannage

| Symptôme | Cause probable |
|---|---|
| Le conteneur s'arrête avec « variable(s) manquante(s) » | `.env` incomplet → relire l'étape 2b / 2c |
| « n'est pas une empreinte bcrypt valide » | `WEB_PASSWORD_HASH` modifié à la main sans apostrophes → relancer `./definir-mot-de-passe.sh` |
| Page « Tracker injoignable » | L'ESP32 est éteint, hors WiFi, en cours de flashage, ou a changé d'IP (voir le bail DHCP) |
| Pas de certificat HTTPS | DNS pas encore propagé, ou ports 80/443 non redirigés vers le serveur. `docker compose logs tracker-web` donne la raison exacte |
| « port is already allocated » | Un autre service utilise déjà 80/443 sur le serveur → me demander, il faudra passer par le reverse proxy existant |
| Modèle 3D absent, page figée | Le **navigateur** n'a pas accès à Internet : Three.js est chargé depuis un CDN (comportement d'origine du firmware) |

## 7. Bon à savoir

- Le petit serveur web de l'ESP32 traite une requête à la fois, et la page
  l'interroge chaque seconde : évite de laisser beaucoup d'onglets ouverts
  en même temps.
- Les commandes du firmware sont des requêtes `GET` (`/monter`, `/gauche`…),
  comme dans la version d'origine. Le mot de passe empêche tout accès
  extérieur ; une évolution future pourrait les passer en `POST`.
- Versions testées : Caddy 2.10.2, core ESP32 3.3.11.
