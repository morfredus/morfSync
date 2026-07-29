#!/usr/bin/env bash
# Resynchronise la copie vendorée de morfdeploy dans third_party/morf/morfdeploy
# depuis morfTools. morfSync n'embarque PAS morfBeacon (il a sa propre annonce de
# présence, hsh::Beacon) : seul le coeur de déploiement est vendoré ici.
#
# Source par défaut : le dossier parent du projet (ex. 01-Travail/).
# Surcharge possible : MORF_SRC_BASE=/chemin/vers/les/depots scripts/sync-morf.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"           # racine du projet
SRC_BASE="${MORF_SRC_BASE:-$(cd "$ROOT/.." && pwd)}"

# Le coeur de deploiement (morfdeploy) vient de morfTools et n'a ni include/ ni
# src/ : c'est un paquet Python, copie tel quel. Sans cette resynchronisation,
# la copie vendoree derive du jour ou morfTools evolue -- « morf doctor » le
# signale, mais autant ne pas creer l'ecart.
if [ -d "$SRC_BASE/morfTools" ]; then
  TOOLS_SRC="$SRC_BASE/morfTools"
else
  TOOLS_SRC="$SRC_BASE/morfTools_travail"
fi
DEPLOY_SRC="$TOOLS_SRC/lib/morfdeploy"
DEPLOY_DST="$ROOT/third_party/morf/morfdeploy"
if [ -d "$DEPLOY_SRC" ]; then
  rm -rf "$DEPLOY_DST"
  mkdir -p "$DEPLOY_DST"
  cp -r "$DEPLOY_SRC/." "$DEPLOY_DST/"
  find "$DEPLOY_DST" -name __pycache__ -type d -prune -exec rm -rf {} +
  echo "OK  morfdeploy"
else
  echo "!! Source introuvable pour morfdeploy : $DEPLOY_SRC" >&2
  echo "   (definir MORF_SRC_BASE si les depots sont ailleurs)" >&2
  exit 1
fi

echo "Synchronisation terminée. Le CMakeLists vendoré n'est pas modifié."
