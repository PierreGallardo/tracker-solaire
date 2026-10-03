#!/bin/sh
# Compile le firmware du tracker. Le croquis est monte (lecture seule) dans
# /work/tracker_web_control54 ; le binaire est ecrit dans /out.
set -eu

SKETCH="${SKETCH_DIR:-/work/tracker_web_control54}"
OUT="${OUT_DIR:-/out}"
# min_spiffs = partition applicative de 1,9 Mo avec OTA (schema deja flashe sur la carte)
FQBN="${FQBN:-esp32:esp32:esp32:PartitionScheme=min_spiffs}"

if [ ! -f "$SKETCH/tracker_web_control54.ino" ]; then
  echo "ERREUR : tracker_web_control54.ino introuvable dans $SKETCH" >&2
  echo "Verifie le volume monte dans docker-compose.yml." >&2
  exit 1
fi

echo "=== Compilation ($FQBN) ==="
arduino-cli compile --fqbn "$FQBN" --build-path "$OUT" "$SKETCH"

BIN="$OUT/tracker_web_control54.ino.bin"
echo
echo "=== Binaire produit ==="
ls -l "$BIN"
