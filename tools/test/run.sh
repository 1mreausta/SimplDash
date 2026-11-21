#!/usr/bin/env bash
# SimplDash testid arvutis:  tools/test/run.sh   (vajab LVGL-i objektifaile: tools/preview/build.sh korra)
set -euo pipefail
cd "$(dirname "$0")"
P=../preview
L=../../managed_components/lvgl__lvgl
M=../../main
[ -f $P/obj/.done ] || { echo "Käivita enne tools/preview/build.sh (kompileerib LVGL-i)"; exit 1; }
clang -O1 -w -DDASH_HOST -DLV_CONF_INCLUDE_SIMPLE -I$P -I$P/stub -I$L -I$L/src -I$M \
  test.c $P/fontblob.s $M/dash_ui.c $M/dash_common.c $M/boot_logo.c $M/needles.c $M/page_*.c $M/maxxecu.c \
  $M/settings.c $M/store.c $M/alarms.c $M/stats.c $M/logger.c $M/can_ecu.c $M/lap_core.c $M/wifi_web.c $M/diag.c \
  $M/web_api.c $M/web_page.c $M/demo.c $M/gps.c $M/lamps.c $P/obj/*.o -o test_bin
./test_bin
