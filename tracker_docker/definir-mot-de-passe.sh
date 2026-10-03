#!/bin/sh
# Definit l'identifiant et le mot de passe de l'interface web du tracker.
#
# Le mot de passe n'est jamais ecrit en clair : seule son empreinte bcrypt
# est enregistree dans .env (variable WEB_PASSWORD_HASH), entre apostrophes
# pour que Docker Compose ne touche pas aux "$" qu'elle contient.
#
# A relancer pour changer de mot de passe, puis :
#   docker compose up -d tracker-web
set -eu
cd "$(dirname "$0")"

if [ ! -f .env ]; then
  cp .env.example .env
  echo "Fichier .env cree a partir de .env.example."
fi
chmod 600 .env

# --- Identifiant -------------------------------------------------------
actuel=$(sed -n 's/^WEB_USER=//p' .env | head -n 1)
printf "Identifiant de l'interface web [%s] : " "${actuel:-tracker}"
read -r utilisateur
utilisateur=${utilisateur:-${actuel:-tracker}}
case "$utilisateur" in
  *[!A-Za-z0-9._-]*|"")
    echo "ERREUR : lettres, chiffres, point, tiret et souligne uniquement." >&2
    exit 1 ;;
esac

# --- Mot de passe (saisie masquee) -------------------------------------
restaurer() { stty echo 2>/dev/null || true; }
trap restaurer EXIT INT TERM
stty -echo 2>/dev/null || true
printf "Mot de passe (12 caracteres minimum, ne s'affiche pas) : "
read -r mdp1; echo
printf "Confirme le mot de passe : "
read -r mdp2; echo
restaurer

if [ "$mdp1" != "$mdp2" ]; then
  echo "ERREUR : les deux saisies sont differentes." >&2
  exit 1
fi
if [ "${#mdp1}" -lt 12 ]; then
  echo "ERREUR : 12 caracteres minimum (l'interface est accessible depuis Internet)." >&2
  exit 1
fi

# --- Empreinte bcrypt, calculee par Caddy dans l'image tracker-web ------
if ! docker image inspect tracker-web:54 >/dev/null 2>&1; then
  echo "Construction de l'image tracker-web (premiere fois)..."
  docker compose build tracker-web >&2
fi
empreinte=$(printf '%s\n' "$mdp1" \
  | docker compose run --rm --no-deps -T --entrypoint caddy tracker-web hash-password)
mdp1=""; mdp2=""

case "$empreinte" in
  '$2a$'*) ;;
  *) echo "ERREUR : empreinte inattendue, .env non modifie." >&2; exit 1 ;;
esac

# --- Mise a jour de .env (remplace les lignes existantes, sinon ajoute) --
tmp=$(mktemp)
U="$utilisateur" H="$empreinte" awk '
  /^WEB_USER=/          { print "WEB_USER=" ENVIRON["U"]; u = 1; next }
  /^WEB_PASSWORD_HASH=/ { print "WEB_PASSWORD_HASH=\047" ENVIRON["H"] "\047"; h = 1; next }
                        { print }
  END {
    if (!u) print "WEB_USER=" ENVIRON["U"]
    if (!h) print "WEB_PASSWORD_HASH=\047" ENVIRON["H"] "\047"
  }' .env > "$tmp"
cat "$tmp" > .env
rm -f "$tmp"

echo "OK : identifiant \"$utilisateur\" enregistre dans .env (mot de passe stocke sous forme d'empreinte)."
echo "Pour l'appliquer : docker compose up -d tracker-web"
