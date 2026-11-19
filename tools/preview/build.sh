#!/usr/bin/env bash
# Dashi eelvaade arvutis (macOS/Linux, vajab clang-i). Teeb PNG pildid kausta out/.
# Kasutab sama dash_ui.c / maxxecu.c koodi nagu püsivara.
# Eeldus: firmware kaustas on vähemalt korra käivitatud "idf.py build" (LVGL laaditakse alla).
set -euo pipefail
cd "$(dirname "$0")"
L=../../managed_components/lvgl__lvgl
M=../../main
mkdir -p obj out
if [ ! -f obj/.done ]; then
  echo "Kompileerin LVGL-i (esimesel korral ~1 min)…"
  i=0
  while IFS= read -r f; do
    i=$((i+1)); clang -c -O1 -w -DLV_CONF_INCLUDE_SIMPLE -I. -I$L -I$L/src "$f" -o obj/l$i.o &
    if (( i % 16 == 0 )); then wait; fi
  done < <(find $L/src -name '*.c'); wait
  touch obj/.done
fi
clang -O1 -w -DDASH_HOST -DLV_CONF_INCLUDE_SIMPLE -I. -Istub -I$L -I$L/src -I$M \
  host_main.c $M/dash_ui.c $M/dash_common.c $M/boot_logo.c $M/needles.c $M/page_*.c $M/maxxecu.c $M/settings.c $M/store.c $M/alarms.c $M/stats.c $M/logger.c $M/can_ecu.c $M/lap_core.c $M/wifi_web.c $M/web_api.c $M/web_page.c $M/gps.c fontblob.s obj/*.o -o dashsim
rm -f out/*.png
cd out && ../dashsim
for f in *.ppm; do
  if command -v sips >/dev/null; then sips -s format png "$f" --out "${f%.ppm}.png" >/dev/null; rm "$f"; fi
done
ls
