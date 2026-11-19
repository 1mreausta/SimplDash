idf_component_register(
    SRCS "main.c" "maxxecu.c" "can_rx.c" "demo.c" "dash_ui.c" "dash_common.c" "boot_logo.c"
         "page_race.c" "page_gauges.c" "page_sweep.c" "page_street.c" "page_data.c"
         "gps.c" "page_accel.c" "page_lap.c"
    INCLUDE_DIRS "."
    EMBED_FILES "fonts/BarlowCondensed-Bold.ttf"
    REQUIRES esp_driver_twai esp_driver_ledc esp_driver_gpio esp_driver_uart esp_timer esp_lcd nvs_flash
)
