#!/usr/bin/env bash
# Avalda veebisimulaator GitHub Pages'i (haru gh-pages):
#   source ~/esp/tools/emsdk/emsdk_env.sh && tools/web/deploy.sh
# Leht: https://<kasutaja>.github.io/SimplDash/
set -euo pipefail
cd "$(dirname "$0")"
./build.sh dist
REPO_URL=$(git -C ../.. remote get-url origin)
NAME=$(git -C ../.. config user.name); EMAIL=$(git -C ../.. config user.email)
REV=$(git -C ../.. rev-parse --short HEAD)
TMP=$(mktemp -d)
cp dist/index.html dist/simpldash.js dist/simpldash.wasm "$TMP/"
touch "$TMP/.nojekyll"
cd "$TMP"
git init -q -b gh-pages
git add -A
git -c user.name="$NAME" -c user.email="$EMAIL" commit -q -m "Veebisimulaator ($REV)"
git push -q -f "$REPO_URL" gh-pages
echo "Avaldatud: $REV → gh-pages"
rm -rf "$TMP"
