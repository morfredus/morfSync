#!/usr/bin/env bash
#
# update-service.sh — Met à jour le binaire du service HomeServerHub (Linux).
#
# Remplace /usr/local/bin/HomeServerHub par une nouvelle version et redémarre le
# service, sans toucher à la configuration ni aux données. Complément de
# install-service.sh (à utiliser pour la première installation).
#
# Usage :
#   sudo ./scripts/linux/update-service.sh              # utilise build/ (ou build-arm64/)
#   sudo ./scripts/linux/update-service.sh --build      # recompile d'abord (en tant que l'utilisateur)
#   sudo ./scripts/linux/update-service.sh /chemin/vers/HomeServerHub
#
# Flux typique :  git pull && sudo ./scripts/linux/update-service.sh --build

set -euo pipefail

SERVICE_NAME="homeserverhub"
BIN_DEST="/usr/local/bin/HomeServerHub"
CONF_DEST="/etc/homeserverhub/config.json"
UNIT_DEST="/etc/systemd/system/$SERVICE_NAME.service"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RUN_USER="${SUDO_USER:-$(logname 2>/dev/null || echo root)}"

# --- Doit être root -------------------------------------------------------
if [[ "${EUID}" -ne 0 ]]; then
    echo "Ce script doit être lancé avec sudo :  sudo $0 $*" >&2
    exit 1
fi

# --- Le service doit déjà être installé -----------------------------------
if [[ ! -f "$UNIT_DEST" ]]; then
    echo "Service '$SERVICE_NAME' non installé." >&2
    echo "Lance d'abord :  sudo ./scripts/linux/install-service.sh" >&2
    exit 1
fi

# --- Recompilation optionnelle (en tant que l'utilisateur, pas root) ------
BIN_SRC=""
if [[ "${1:-}" == "--build" ]]; then
    echo "Recompilation (utilisateur $RUN_USER)…"
    sudo -u "$RUN_USER" bash -c "cd '$REPO_ROOT' && cmake --preset linux && cmake --build --preset linux"
    BIN_SRC="$REPO_ROOT/build/HomeServerHub"
elif [[ -n "${1:-}" ]]; then
    BIN_SRC="$1"
fi

# --- Localiser le binaire si non fourni ----------------------------------
if [[ -z "$BIN_SRC" ]]; then
    for candidate in "$REPO_ROOT/build/HomeServerHub" "$REPO_ROOT/build-arm64/HomeServerHub"; do
        if [[ -x "$candidate" ]]; then BIN_SRC="$candidate"; break; fi
    done
fi
if [[ -z "$BIN_SRC" || ! -f "$BIN_SRC" ]]; then
    echo "Binaire introuvable. Compile d'abord (cmake --preset linux && cmake --build --preset linux)" >&2
    echo "ou passe le chemin :  sudo $0 /chemin/vers/HomeServerHub" >&2
    exit 1
fi
echo "Nouveau binaire : $BIN_SRC"

# --- Version avant (via le service en cours) ------------------------------
PORT="$(grep -oE '"port"[^0-9]*[0-9]+' "$CONF_DEST" 2>/dev/null | grep -oE '[0-9]+' || echo 8080)"
version_now() { curl -fs "http://localhost:$PORT/api/health" 2>/dev/null | grep -oE '"version":"[^"]*"' | cut -d'"' -f4; }
OLD_VERSION="$(version_now || true)"

# --- Remplacer le binaire (service arrêté pour éviter « Text file busy ») --
echo "Arrêt du service…"
systemctl stop "$SERVICE_NAME"
install -m 0755 "$BIN_SRC" "$BIN_DEST"
echo "Binaire remplacé : $BIN_DEST"
echo "Redémarrage…"
systemctl start "$SERVICE_NAME"

# --- Vérification ---------------------------------------------------------
sleep 1
NEW_VERSION=""
for _ in $(seq 1 10); do NEW_VERSION="$(version_now || true)"; [[ -n "$NEW_VERSION" ]] && break; sleep 0.5; done

echo
if [[ -n "$NEW_VERSION" ]]; then
    echo "Mise à jour OK : ${OLD_VERSION:-?} -> ${NEW_VERSION}"
else
    echo "Le service ne répond pas encore — voir : journalctl -u $SERVICE_NAME -e" >&2
    systemctl --no-pager --lines=0 status "$SERVICE_NAME" || true
fi
