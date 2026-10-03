#!/bin/sh
# Point d'entree de l'image tracker-web : verifie la configuration AVANT
# de lancer Caddy, pour ne jamais demarrer un site mal protege et pour
# donner un message clair plutot qu'une erreur obscure.
set -eu

manque=""
[ -n "${SITE_ADDRESS:-}" ]      || manque="$manque SITE_ADDRESS"
[ -n "${ESP32_URL:-}" ]         || manque="$manque ESP32_URL"
[ -n "${WEB_USER:-}" ]          || manque="$manque WEB_USER"
[ -n "${WEB_PASSWORD_HASH:-}" ] || manque="$manque WEB_PASSWORD_HASH"

if [ -n "$manque" ]; then
  echo "ERREUR : variable(s) manquante(s) dans tracker_docker/.env :$manque" >&2
  echo "Pour le mot de passe de l'interface : ./definir-mot-de-passe.sh" >&2
  exit 1
fi

# Une empreinte bcrypt commence par $2a$, $2b$ ou $2y$ et fait TOUJOURS
# 60 caracteres. Sinon, c'est presque toujours que des "$" ont ete
# interpretes par Docker Compose (valeur sans apostrophes dans .env : le
# debut "$2a$" survit mais le milieu disparait -- constate en test), ou
# qu'un mot de passe en clair a ete colle a la place de l'empreinte.
case "$WEB_PASSWORD_HASH" in
  '$2a$'*|'$2b$'*|'$2y$'*) longueur_ok=$([ "${#WEB_PASSWORD_HASH}" -eq 60 ] && echo 1 || echo 0) ;;
  *) longueur_ok=0 ;;
esac
case "$longueur_ok" in
  1) ;;
  *)
    echo "ERREUR : WEB_PASSWORD_HASH n'est pas une empreinte bcrypt valide." >&2
    echo "Elle doit commencer par \$2a\$ et etre entre apostrophes dans .env :" >&2
    echo "  WEB_PASSWORD_HASH='\$2a\$14\$...'" >&2
    echo "Le plus simple : relance ./definir-mot-de-passe.sh" >&2
    exit 1
    ;;
esac

echo "Interface du tracker : $SITE_ADDRESS  ->  ESP32 $ESP32_URL (utilisateur : $WEB_USER)"
exec caddy run --config /etc/caddy/Caddyfile --adapter caddyfile
