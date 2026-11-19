idf_component_register(
    SRCS "main.c" "maxxecu.c" "can_rx.c" "demo.c" "dash_ui.c" "dash_common.c" "boot_logo.c"
         "page_race.c" "page_gauges.c" "page_sweep.c" "page_street.c" "page_data.c"
         "gps.c" "page_accel.c" "page_lap.c" "page_classic.c" "needles.c"
         "settings.c" "store.c" "alarms.c" "stats.c" "logger.c" "can_ecu.c" "lap_core.c"
         "page_track.c" "page_ecu.c" "page_settings.c" "wifi_web.c" "web_api.c" "web_page.c"
    INCLUDE_DIRS "."
    EMBED_FILES "fonts/BarlowCondensed-Bold.ttf"
    REQUIRES esp_driver_twai esp_driver_ledc esp_driver_gpio esp_driver_uart esp_timer esp_lcd nvs_flash
             esp_http_server app_update esp_netif esp_event fatfs vfs
)
# dash_config.h tabelites võib viimased väljad (nt näidiku "offset") ära jätta – need on 0
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers -Wno-format-truncation)
