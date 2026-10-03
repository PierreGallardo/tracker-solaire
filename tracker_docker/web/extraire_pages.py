#!/usr/bin/env python3
"""
Extrait les pages HTML embarquees dans le firmware de l'ESP32.

Dans le firmware, chaque page est une chaine C++ brute stockee en flash :

    static const char PAGE_INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>...)rawliteral";

Ce script recopie le contenu entre R"rawliteral( et )rawliteral" dans un
fichier .html, octet pour octet. Il est lance automatiquement a la
construction de l'image Docker "tracker-web" : le firmware reste la SEULE
source des pages. Modifier une page = la modifier dans le .h (comme
avant), puis reconstruire l'image.

Usage :
    python3 extraire_pages.py <dossier_du_croquis> <dossier_de_sortie>

Echoue (code 1) si une page attendue est introuvable : mieux vaut une
construction qui casse qu'un serveur qui servirait une page perimee.
"""

import pathlib
import re
import sys

# Nom de la constante C++  ->  (fichier source, fichier produit)
PAGES = {
    "PAGE_INDEX_HTML":  ("PageWeb.h",    "index.html"),
    "PAGE_CALIB_HTML":  ("PageCalib.h",  "calibration.html"),
    "PAGE_CODEUR_HTML": ("PageCodeur.h", "codeur.html"),
}

MOTIF = r'%s\[\]\s*PROGMEM\s*=\s*R"rawliteral\((.*?)\)rawliteral"'


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 1

    croquis = pathlib.Path(sys.argv[1])
    sortie = pathlib.Path(sys.argv[2])
    sortie.mkdir(parents=True, exist_ok=True)

    erreurs = 0
    for constante, (source, cible) in PAGES.items():
        chemin = croquis / source
        if not chemin.is_file():
            print(f"ERREUR : {chemin} introuvable", file=sys.stderr)
            erreurs += 1
            continue

        texte = chemin.read_text(encoding="utf-8")
        trouve = re.search(MOTIF % re.escape(constante), texte, re.S)
        if not trouve:
            print(f"ERREUR : {constante} introuvable dans {source}", file=sys.stderr)
            erreurs += 1
            continue

        html = trouve.group(1)
        (sortie / cible).write_text(html, encoding="utf-8")
        print(f"{source:<14} -> {cible:<17} {len(html.encode('utf-8')):>7} octets")

    return 1 if erreurs else 0


if __name__ == "__main__":
    sys.exit(main())
