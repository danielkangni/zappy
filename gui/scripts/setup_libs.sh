#!/usr/bin/env bash
# Installe les bibliothèques header-only (nlohmann/json, tinygltf) comme une
# vraie dépendance : dans un include path, PAS dans le repo. Récupérées depuis
# leur dépôt public, puis posées dans $PREFIX/include.
#
#   ./setup_libs.sh                       # -> ~/.local/include (défaut, sans sudo)
#   PREFIX=/usr/local sudo ./setup_libs.sh # -> install système (comme SFML/GLM)
#
# Le Makefile l'appelle tout seul au 1er `make`, donc rien à faire à la main.
set -euo pipefail

PREFIX="${PREFIX:-$HOME/.local}"
INCDIR="$PREFIX/include"
BASE="https://raw.githubusercontent.com/Knight-coder-19/gltf-json-header-libs/master"
FILES=(json.hpp tiny_gltf.h tinygltf_json.hpp)

mkdir -p "$INCDIR"
for f in "${FILES[@]}"; do
    [ -s "$INCDIR/$f" ] && { echo "  $f déjà installé ($INCDIR)"; continue; }
    echo "  installation de $f -> $INCDIR ..."
    tmp="$(mktemp)"
    curl -fsSL "$BASE/$f" -o "$tmp"
    install -m 644 "$tmp" "$INCDIR/$f"
    rm -f "$tmp"
done
echo "OK — headers tiers installés dans $INCDIR (comme une lib système)"
