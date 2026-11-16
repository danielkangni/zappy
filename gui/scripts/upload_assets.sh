#!/usr/bin/env bash
# Pousse gui/assets/ vers le bucket R2 (CDN). Lit les identifiants dans .env.local.
set -euo pipefail
cd "$(dirname "$0")/.."          # -> gui/ (le script vit dans gui/scripts/)
set -a; . ./.env.local; set +a
RC="${RCLONE:-/home/knight-coder/bin/rclone}"
"$RC" copy assets/ ":s3:$R2_BUCKET" \
    --s3-provider=Cloudflare \
    --s3-access-key-id="$R2_ACCESS_KEY_ID" \
    --s3-secret-access-key="$R2_SECRET_ACCESS_KEY" \
    --s3-endpoint="$R2_ENDPOINT" \
    --s3-no-check-bucket \
    --exclude '*.glbc' --exclude '*.glbca' --exclude '.env.local' \
    --transfers=8 --checkers=16 --progress --stats=5s
