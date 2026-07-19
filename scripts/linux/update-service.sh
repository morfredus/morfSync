#!/usr/bin/env bash
#
# update-service.sh — Met à jour le binaire du service morfSync (Linux).
#
# Remplace /usr/local/bin/morfSync par une nouvelle version et redémarre le
# service, sans toucher à la configuration ni aux données. Complément de
# install-service.sh (à utiliser pour la première installation).
#
# Usage :
#   sudo ./scripts/linux/update-service.sh                    # utilise build/ (ou build-arm64/)
#   sudo ./scripts/linux/update-service.sh --build            # git pull + build neuf (preset auto)
#   sudo ./scripts/linux/update-service.sh --build linux-arm64 # forcer un preset
#   sudo ./scripts/linux/update-service.sh /chemin/vers/morfSync
#
# --build auto-détecte le preset : linux-arm64 sur un Pi 64 bits, linux sinon.
# Flux typique sur le Pi :  sudo ./scripts/linux/update-service.sh --build

set -euo pipefail

# Analyse des options, INDEPENDANTE de leur ordre. Le script lisait --build en
# 1re position et son preset en 2e ; ajouter une option devant aurait casse cette
# lecture. Les drapeaux sont donc extraits ici, et le preset est le premier
# argument qui n'est pas un drapeau.
NO_CONFIG=0
DO_BUILD=0
PRESET_ARG=""
for arg in "$@"; do
    case "$arg" in
        --no-config) NO_CONFIG=1 ;;
        --build)     DO_BUILD=1 ;;
        --*)         echo "Option inconnue : $arg" >&2; exit 1 ;;
        *)           [[ -z "$PRESET_ARG" ]] && PRESET_ARG="$arg" ;;
    esac
done

SERVICE_NAME="morfsync"
BIN_DEST="/usr/local/bin/morfSync"
CONF_DEST="/etc/morfsync/config.json"
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
# --build [preset] : récupère le code (git pull) PUIS reconstruit à neuf.
# Le build neuf est indispensable : après un simple git pull, CMake ne rebâtit
# pas toujours (le numéro de version resterait périmé dans le binaire).
# Le preset est auto-détecté selon l'architecture (linux-arm64 sur un Pi 64 bits,
# linux sinon), ou forcé en 2e argument : --build linux-arm64-cross
BIN_SRC=""
if [[ $DO_BUILD -eq 1 ]]; then
    PRESET="$PRESET_ARG"
    if [[ -z "$PRESET" ]]; then
        case "$(uname -m)" in
            aarch64|arm64) PRESET="linux-arm64" ;;   # Raspberry Pi 64 bits
            *)             PRESET="linux" ;;
        esac
    fi
    case "$PRESET" in
        linux)             BUILD_DIR="build" ;;
        linux-arm64)       BUILD_DIR="build-arm64" ;;
        linux-arm64-cross) BUILD_DIR="build-arm64-cross" ;;
        *) echo "Preset inconnu : $PRESET (attendu : linux, linux-arm64, linux-arm64-cross)" >&2; exit 1 ;;
    esac
    echo "Mise à jour du code + recompilation propre (preset $PRESET, utilisateur $RUN_USER)…"
    sudo -u "$RUN_USER" bash -c "cd '$REPO_ROOT' && git pull --ff-only && rm -rf '$BUILD_DIR' && cmake --preset '$PRESET' && cmake --build --preset '$PRESET'"
    BIN_SRC="$REPO_ROOT/$BUILD_DIR/morfSync"
elif [[ -n "$PRESET_ARG" ]]; then
    # Sans --build, le premier argument non-drapeau est un chemin de binaire.
    BIN_SRC="$PRESET_ARG"
fi

# --- Localiser le binaire si non fourni ----------------------------------
if [[ -z "$BIN_SRC" ]]; then
    for candidate in "$REPO_ROOT/build/morfSync" "$REPO_ROOT/build-arm64/morfSync"; do
        if [[ -x "$candidate" ]]; then BIN_SRC="$candidate"; break; fi
    done
fi
if [[ -z "$BIN_SRC" || ! -f "$BIN_SRC" ]]; then
    echo "Binaire introuvable. Compile d'abord (cmake --preset linux && cmake --build --preset linux)" >&2
    echo "ou passe le chemin :  sudo $0 /chemin/vers/morfSync" >&2
    exit 1
fi
echo "Nouveau binaire : $BIN_SRC  [$("$BIN_SRC" --version 2>/dev/null || echo 'version ?')]"

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
# --- Rafraîchir l'unité systemd ------------------------------------------
# Une modification du fichier .service dans le dépôt ne parvenait jamais à
# /etc/systemd/system : le service continuait de tourner avec l'ancienne
# définition, sans que rien ne le signale.
if [[ -f "$SCRIPT_DIR/morfsync.service" ]]; then
    NEW_UNIT="$(mktemp)"
    sed -e "s/__RUN_USER__/$RUN_USER/g" "$SCRIPT_DIR/morfsync.service" > "$NEW_UNIT"
    if ! cmp -s "$NEW_UNIT" "$UNIT_DEST"; then
        echo "Unité systemd modifiée : mise à jour."
        install -m 0644 "$NEW_UNIT" "$UNIT_DEST"
        systemctl daemon-reload
    fi
    rm -f "$NEW_UNIT"
fi

# --- Compléter la configuration ------------------------------------------
# Les valeurs déjà en place ne sont JAMAIS modifiées, mais les paramètres
# APPARUS depuis l'installation sont ajoutés. Sans cela, une version
# introduisant un paramètre le laissait absent indéfiniment et la fonction
# correspondante ne s'activait jamais, en silence.
EXAMPLE_FILE="$REPO_ROOT/config.example.json"
if [[ $NO_CONFIG -eq 0 && -f "$EXAMPLE_FILE" ]]; then
    if [[ ! -f "$CONF_DEST" ]]; then
        mkdir -p "$(dirname "$CONF_DEST")"
        install -m 0644 "$EXAMPLE_FILE" "$CONF_DEST"
        echo "Config absente : copiée depuis l'exemple -> $CONF_DEST (à adapter)."
    elif command -v python3 >/dev/null 2>&1; then
        BACKUP="$CONF_DEST.bak-$(date +%Y%m%d-%H%M%S)"
        cp "$CONF_DEST" "$BACKUP"
        ADDED="$(python3 "$SCRIPT_DIR/merge-config.py" "$EXAMPLE_FILE" "$CONF_DEST" || true)"
        if [[ -n "$ADDED" ]]; then
            echo
            echo "Nouveaux paramètres ajoutés à $CONF_DEST :"
            echo "$ADDED" | sed 's/^/    /'
            echo "  (valeurs existantes inchangées ; sauvegarde : $BACKUP)"
            echo "  À RENSEIGNER si besoin avant que la fonction correspondante s'active."
            echo
        else
            rm -f "$BACKUP"
        fi
    else
        echo "python3 absent : configuration non complétée (voir $EXAMPLE_FILE)." >&2
    fi
fi

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
