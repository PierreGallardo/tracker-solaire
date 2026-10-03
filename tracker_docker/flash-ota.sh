#!/bin/sh
# Envoie le binaire compile a l'ESP32 par WiFi (OTA).
# Variables attendues (fichier .env) : OTA_IP, OTA_PASSWORD, HOST_IP
set -eu

: "${OTA_IP:?definis OTA_IP dans .env (ex. 192.168.1.60)}"
: "${OTA_PASSWORD:?definis OTA_PASSWORD dans .env}"
: "${HOST_IP:?definis HOST_IP dans .env : l'adresse LAN de CE serveur (ex. 192.168.1.77)}"

BIN="${BIN:-/out/tracker_web_control54.ino.bin}"
HOST_PORT="${HOST_PORT:-3232}"

if [ ! -f "$BIN" ]; then
  echo "ERREUR : $BIN introuvable. Lance d'abord la compilation (tracker-build)." >&2
  exit 1
fi

ESPOTA="$(find "$ARDUINO_DIRECTORIES_DATA/packages/esp32" -name espota.py | head -n 1)"
if [ -z "$ESPOTA" ]; then
  echo "ERREUR : espota.py introuvable dans le core ESP32." >&2
  exit 1
fi

echo "=== Envoi OTA vers $OTA_IP (retour attendu sur $HOST_IP:$HOST_PORT) ==="
# -I / -P : l'ESP32 se reconnecte au serveur pendant le transfert ; sans adresse
# et port explicites, le transfert echoue (constate sur ce projet : "No response").
python3 "$ESPOTA" -i "$OTA_IP" -p 3232 -I "$HOST_IP" -P "$HOST_PORT" \
    --auth="$OTA_PASSWORD" -f "$BIN" -r

echo
echo "Fini. Verifie que la carte a redemarre : l'uptime de /status doit etre faible."
