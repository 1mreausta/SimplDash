idf_component_register(
    SRCS "main.c" "maxxecu.c" "can_rx.c" "demo.c" "dash_ui.c" "dash_common.c" "boot_logo.c"
         "page_race.c" "page_gauges.c" "page_sweep.c" "page_street.c" "page_data.c"
    INCLUDE_DIRS "."
    EMBED_FILES "fonts/BarlowCondensed-Bold.ttf"
    REQUIRES esp_driver_twai esp_driver_ledc esp_driver_gpio esp_timer esp_lcd nvs_flash
)
