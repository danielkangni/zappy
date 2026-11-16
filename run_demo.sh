#!/usr/bin/env bash
# Lance la démo complète : serveur + 3 équipes + GUI.
# On lance UNE seule IA par équipe : c'est la REINE de l'équipe. Elle ne fait que
# survivre et forker pour maintenir l'effectif (rebâtit sous 7, plafond 9) ; ses
# ouvriers évoluent et coopèrent entre équipes (incantations cross-team par
# niveau). Quand tu appuies sur START après l'intro, la GUI rejoint la partie.
#
# Usage : ./run_demo.sh [MOVE_SEC] [PORT]
#   MOVE_SEC  durée d'un déplacement en secondes  (défaut 0.5 -> ~2 tuiles/s ;
#             plus haut = plus lent)
#   PORT      port du serveur                      (défaut 4242)
#
# À lancer depuis ton propre terminal (pas via l'agent) pour que la fenêtre
# reste ouverte tant que tu ne la fermes pas. Ctrl-C arrête tout proprement.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AI_DELAY="${1:-0.5}"
PORT="${2:-4242}"
WIDTH=20
HEIGHT=20
PLAYERS_PER_TEAM=9
TEAMS=(team1 team2)

pids=()
cleanup() {
    echo
    echo "[run_demo] arrêt..."
    for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "[run_demo] serveur port $PORT — ${#TEAMS[@]} équipes x $PLAYERS_PER_TEAM joueurs (carte ${WIDTH}x${HEIGHT})"
"$ROOT/server/zappy_server" -p "$PORT" -x "$WIDTH" -y "$HEIGHT" \
    -n "${TEAMS[@]}" -c "$PLAYERS_PER_TEAM" -f 100 &
pids+=($!)
sleep 1

echo "[run_demo] IA : 1 client par équipe (la chaîne de remplissage peuple jusqu'à -c=$PLAYERS_PER_TEAM ; délai=${AI_DELAY}s/déplacement)"
for team in "${TEAMS[@]}"; do
    ( cd "$ROOT/ai" && ZAPPY_AI_DELAY="$AI_DELAY" \
        python3 zappy_ai -p "$PORT" -n "$team" -h localhost ) &
    pids+=($!)
done
sleep 1

# Pas de ZAPPY_AUTOSTART : la GUI joue l'intro, puis START rejoint la partie.
echo "[run_demo] GUI — intro puis START pour rejoindre 127.0.0.1:$PORT (ferme la fenêtre / Ctrl-C pour quitter)"
cd "$ROOT/gui"
./zappy_gui -p "$PORT" -h localhost
