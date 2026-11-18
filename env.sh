# Laadi ESP-IDF keskkond:  source env.sh
# Eeldab ESP-IDF 6.1 kaustas ~/esp/esp-idf (muuda IDF_PATH, kui see on mujal).
IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}"
[ -d "$HOME/esp/tools/espressif" ] && export IDF_TOOLS_PATH="$HOME/esp/tools/espressif"
[ -d "$HOME/esp/tools/shim" ] && export PATH="$HOME/esp/tools/shim:$PATH"
. "$IDF_PATH/export.sh" > /dev/null
echo "ESP-IDF $(idf.py --version 2>/dev/null) valmis."
