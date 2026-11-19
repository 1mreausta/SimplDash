#!/usr/bin/env bash
# SimplDash simulaator: kompileerib sama UI koodi arvutile ja avab selle brauseris.
# Kasutus:  tools/sim/run.sh [port]      (vaikimisi 8090) → ava http://localhost:8090
# Vajab: macOS + clang (Xcode Command Line Tools). Eeldus: "idf.py build" on korra tehtud (LVGL allalaaditud).
set -euo pipefail
cd "$(dirname "$0")"
P=../preview
L=../../managed_components/lvgl__lvgl
M=../../main
if [ ! -f $P/obj/.done ]; then
  echo "Kompileerin LVGL-i (esimesel korral ~1 min)…"
  mkdir -p $P/obj
  i=0
  while IFS= read -r f; do
    i=$((i+1)); clang -c -O2 -w -DLV_CONF_INCLUDE_SIMPLE -I$P -I$L -I$L/src "$f" -o $P/obj/l$i.o &
    if (( i % 16 == 0 )); then wait; fi
  done < <(find $L/src -name '*.c'); wait
  touch $P/obj/.done
fi
VER=$(sed -n 's/.*SIMPLDASH_VERSION "\(.*\)".*/\1/p' "$M/version.h")
sed "s/__SIMPLDASH_VERSION__/$VER/g" index.html > index.gen.html
clang -O2 -w -DDASH_HOST -DLV_CONF_INCLUDE_SIMPLE -I$P -I$P/stub -I$L -I$L/src -I$M \
  sim_main.c html.s $P/fontblob.s \
  $M/dash_ui.c $M/dash_common.c $M/boot_logo.c $M/page_*.c $M/maxxecu.c $M/demo.c \
  $P/obj/*.o -o simpldash_sim
exec ./simpldash_sim "${1:-8090}"
