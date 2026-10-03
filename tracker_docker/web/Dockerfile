# ============================================================
# Image "tracker-web" : sert l'interface du tracker depuis le serveur,
# en HTTPS et protegee par mot de passe, et relaie les commandes a
# l'ESP32 sur le reseau local.
#
# Construite depuis la RACINE du depot (voir docker-compose.yml) pour
# pouvoir lire les pages dans tracker_web_control54/. Le fichier
# .dockerignore a la racine ne laisse passer QUE les 3 pages et ce
# dossier : Secrets.h (mots de passe WiFi/OTA) ne peut pas entrer dans
# l'image, meme par erreur.
# ============================================================

# --- Etape 1 : extraction des pages HTML depuis le firmware -----------
FROM python:3.12-alpine AS pages
WORKDIR /build
COPY tracker_docker/web/extraire_pages.py .
COPY tracker_web_control54/PageWeb.h \
     tracker_web_control54/PageCalib.h \
     tracker_web_control54/PageCodeur.h \
     croquis/
RUN python3 extraire_pages.py croquis /pages

# --- Etape 2 : image finale, Caddy seul (python n'y est pas) ----------
# Version testee : 2.10.2
FROM caddy:2.10-alpine
COPY --from=pages /pages /srv
COPY tracker_docker/web/Caddyfile /etc/caddy/Caddyfile
COPY tracker_docker/web/demarrer.sh /usr/local/bin/demarrer.sh
# Garde-fou fins de ligne Windows (meme raison que dans l'image de build)
RUN sed -i 's/\r$//' /usr/local/bin/demarrer.sh /etc/caddy/Caddyfile \
 && chmod +x /usr/local/bin/demarrer.sh

EXPOSE 80 443 443/udp
ENTRYPOINT ["/usr/local/bin/demarrer.sh"]
