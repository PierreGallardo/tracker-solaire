# ============================================================
# Image "tracker-firmware-build" : chaine de compilation + flashage
# OTA du firmware du tracker solaire ESP32.
#
# CE N'EST PAS UNE IMAGE QUI "FAIT TOURNER" LE TRACKER : le firmware
# s'execute sur l'ESP32 et sert lui-meme l'interface web. L'image
# contient seulement les OUTILS pour compiler le code et l'envoyer
# a la carte par WiFi, de facon reproductible, depuis le serveur.
#
# Le code source n'est PAS copie dans l'image : il est monte au
# lancement. Config.h contient le mot de passe WiFi et celui de l'OTA,
# qui ne doivent jamais se retrouver dans une couche d'image.
# ============================================================

FROM debian:bookworm-slim

# Versions epinglees : ce sont celles avec lesquelles le firmware a ete
# compile et teste. Laisser vide ARDUINO_CLI_VERSION = derniere version.
ARG ARDUINO_CLI_VERSION=
ARG ESP32_CORE=3.3.11

ENV DEBIAN_FRONTEND=noninteractive \
    ARDUINO_DIRECTORIES_DATA=/opt/arduino/data \
    ARDUINO_DIRECTORIES_USER=/opt/arduino/user \
    ARDUINO_DIRECTORIES_DOWNLOADS=/opt/arduino/staging \
    ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS=https://espressif.github.io/arduino-esp32/package_esp32_index.json

# python3 : necessaire a espota.py (envoi OTA) et a certains outils du core ESP32.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates curl git xz-utils bzip2 python3 python3-serial \
 && rm -rf /var/lib/apt/lists/*

# arduino-cli
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
    | BINDIR=/usr/local/bin sh -s ${ARDUINO_CLI_VERSION} \
 && arduino-cli version

# Core ESP32 (plusieurs centaines de Mo : c'est l'essentiel du poids de l'image)
RUN arduino-cli core update-index \
 && arduino-cli core install esp32:esp32@${ESP32_CORE}

# Bibliotheques, aux versions utilisees par le projet
RUN arduino-cli lib install \
      "ArduinoJson@7.4.3" \
      "Adafruit BMP280 Library@3.0.0" \
      "Adafruit Unified Sensor@1.1.15" \
      "Adafruit BusIO@1.17.4" \
      "RTClib@2.1.4"

COPY build.sh flash-ota.sh /usr/local/bin/
# Garde-fou : un script reenregistre sous Windows (fins de ligne CRLF) ne
# s'executerait pas sous Linux. On retire tout CR au moment de la construction.
RUN sed -i 's/\r$//' /usr/local/bin/build.sh /usr/local/bin/flash-ota.sh \
 && chmod +x /usr/local/bin/build.sh /usr/local/bin/flash-ota.sh

WORKDIR /work
CMD ["build.sh"]
