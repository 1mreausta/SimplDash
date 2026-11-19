#!/usr/bin/env bash
# SimplDash veebisimulaator: kompileerib UI WebAssembly'ks (Emscripten).
# Kasutus:  tools/web/build.sh [väljundkaust]   (vaikimisi tools/web/dist)
# Vajab: Emscripten (emcc) PATH-is  – nt  source ~/esp/tools/emsdk/emsdk_env.sh
#        LVGL lähtekood: firmware/managed_components/lvgl__lvgl ("idf.py build" toob selle)
#        või keskkonnamuutuja LVGL_DIR.
set -euo pipefail
cd "$(dirname "$0")"
OUT="${1:-dist}"
M=../../main
P=../preview
L="${LVGL_DIR:-../../managed_components/lvgl__lvgl}"
OBJ=obj
command -v emcc >/dev/null || { echo "emcc puudub – source ~/esp/tools/emsdk/emsdk_env.sh"; exit 1; }
[ -d "$L/src" ] || { echo "LVGL puudub ($L) – käivita firmware kaustas 'idf.py build' või sea LVGL_DIR"; exit 1; }
mkdir -p "$OBJ" "$OUT"

CFLAGS="-O2 -w -DLV_CONF_INCLUDE_SIMPLE -DDASH_HOST -DDASH_FONT_C_ARRAY -I$P -I$P/stub -I$L -I$L/src -I$M"

if [ ! -f "$OBJ/.lvgl_done" ]; then
  echo "Kompileerin LVGL-i WebAssembly'ks (esimesel korral mõni minut)…"
  i=0
  while IFS= read -r f; do
    i=$((i+1)); emcc -c $CFLAGS "$f" -o "$OBJ/l$i.o" &
    if (( i % 8 == 0 )); then wait; fi
  done < <(find "$L/src" -name '*.c'); wait
  touch "$OBJ/.lvgl_done"
fi

# font C-massiiviks
python3 - "$M/fonts/BarlowCondensed-Bold.ttf" > "$OBJ/font_data.c" <<'PY'
import sys
b = open(sys.argv[1], 'rb').read()
print('const unsigned char dash_font_ttf[] = {')
for i in range(0, len(b), 24):
    print(','.join(str(x) for x in b[i:i+24]) + ',')
print('};\nconst unsigned int dash_font_ttf_len = %d;' % len(b))
PY

echo "Kompileerin SimplDash'i…"
emcc $CFLAGS web_main.c "$OBJ/font_data.c" \
  $M/dash_ui.c $M/dash_common.c $M/boot_logo.c $M/page_*.c $M/maxxecu.c $M/demo.c \
  "$OBJ"/l*.o -o "$OUT/simpldash.js" \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sSTACK_SIZE=1048576 \
  -sEXPORTED_FUNCTIONS=_main,_sim_pointer,_sim_key,_sim_state,_sim_fb,_sim_frame_no,_sim_bench \
  -sEXPORTED_RUNTIME_METHODS=HEAPU16 -sENVIRONMENT=web
cp index.html "$OUT/index.html"
ls -la "$OUT"
