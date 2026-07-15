#!/usr/bin/env bash
#
# install-service.sh — Installe HomeServerHub en service systemd (Linux/Raspberry Pi).
#
# Usage :
#   sudo ./scripts/linux/install-service.sh [chemin/vers/HomeServerHub]
#   sudo ./scripts/linux/install-service.sh --uninstall
#
# Sans argument, le script cherche le binaire dans build/ puis build-arm64/.
# Il installe le binaire dans /usr/local/bin, la configuration dans
# /etc/homeserverhub/, le service dans systemd, puis démarre tout.

set -euo pipefail

SERVICE_NAME="homeserverhub"
BIN_DEST="/usr/local/bin/HomeServerHub"
CONF_DIR="/etc/homeserverhub"
CONF_DEST="$CONF_DIR/config.json"
UNIT_DEST="/etc/systemd/system/$SERVICE_NAME.service"

# Racine du dépôt = deux niveaux au-dessus de ce script.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# --- Doit être root -------------------------------------------------------
if [[ "${EUID}" -ne 0 ]]; then
    echo "Ce script doit être lancé avec sudo :  sudo $0 $*" >&2
    exit 1
fi

# --- Désinstallation ------------------------------------------------------
if [[ "${1:-}" == "--uninstall" ]]; then
    echo "Désinstallation de $SERVICE_NAME…"
    systemctl disable --now "$SERVICE_NAME" 2>/dev/null || true
    rm -f "$UNIT_DEST"
    systemctl daemon-reload
    echo "Service supprimé. (Binaire $BIN_DEST et config $CONF_DIR conservés.)"
    echo "Pour tout retirer :  sudo rm -f $BIN_DEST ; sudo rm -rf $CONF_DIR"
    exit 0
fi

# --- Localiser le binaire -------------------------------------------------
BIN_SRC="${1:-}"
if [[ -z "$BIN_SRC" ]]; then
    for candidate in "$REPO_ROOT/build/HomeServerHub" "$REPO_ROOT/build-arm64/HomeServerHub"; do
        if [[ -x "$candidate" ]]; then BIN_SRC="$candidate"; break; fi
    done
fi
if [[ -z "$BIN_SRC" || ! -f "$BIN_SRC" ]]; then
    echo "Binaire introuvable. Compile d'abord :" >&2
    echo "    cmake --preset linux && cmake --build --preset linux" >&2
    echo "…ou passe le chemin en argument :  sudo $0 /chemin/vers/HomeServerHub" >&2
    exit 1
fi
echo "Binaire      : $BIN_SRC"

# --- Installer binaire + config -------------------------------------------
install -m 0755 "$BIN_SRC" "$BIN_DEST"
echo "Installé     : $BIN_DEST"

mkdir -p "$CONF_DIR"
if [[ ! -f "$CONF_DEST" ]]; then
    if [[ -f "$REPO_ROOT/config.example.json" ]]; then
        install -m 0644 "$REPO_ROOT/config.example.json" "$CONF_DEST"
    else
        cat > "$CONF_DEST" <<'JSON'
{
  "host": "0.0.0.0",
  "port": 8080,
  "token": ""
}
JSON
    fi
    # dataDir volontairement absent : le service tourne avec StateDirectory=
    # homeserverhub, donc HomeServerHub écrit dans /var/lib/homeserverhub
    # automatiquement (variable $STATE_DIRECTORY, cf. paths.cpp).
    echo "Config créée : $CONF_DEST  (édite-la pour changer le port ou ajouter un token)"
else
    echo "Config       : $CONF_DEST  (existante, conservée)"
fi

# --- Installer et démarrer le service -------------------------------------
install -m 0644 "$SCRIPT_DIR/homeserverhub.service" "$UNIT_DEST"
systemctl daemon-reload
systemctl enable --now "$SERVICE_NAME"

echo
echo "Service '$SERVICE_NAME' installé et démarré."
sleep 1
systemctl --no-pager --lines=0 status "$SERVICE_NAME" || true

# --- Vérification ---------------------------------------------------------
PORT="$(grep -oE '"port"[^0-9]*[0-9]+' "$CONF_DEST" | grep -oE '[0-9]+' || echo 8080)"
echo
echo "Vérification :  curl http://localhost:$PORT/api/health"
if command -v curl >/dev/null; then
    sleep 1
    curl -fs "http://localhost:$PORT/api/health" && echo || echo "(pas encore de réponse — voir : journalctl -u $SERVICE_NAME -e)"
fi

IP="$(hostname -I 2>/dev/null | awk '{print $1}')"
echo
echo "Depuis un autre poste du réseau :  curl http://${IP:-<ip-du-pi>}:$PORT/api/health"
echo "Rappel firewall (si ufw actif)  :  sudo ufw allow $PORT/tcp"
