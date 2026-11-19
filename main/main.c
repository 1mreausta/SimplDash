/*
 * SimplDash – MaxxECU CAN näidikuplokk
 * Plaat: Guition JC1060P470C-I-W-Y (ESP32-P4 + C6, 7" 1024x600)
 */
#include "can_rx.h"
#include "dash_ui.h"
#include "demo.h"
#include "dev_display_lcd.h"
#include "gps.h"
#include "dev_lcd_touch.h"
#include "dev_ledc_ctrl.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_board_manager.h"
#include "esp_board_periph.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "periph_ledc.h"
#include "sdkconfig.h"
#include "version.h"

static const char *TAG = "simpldash";

static periph_ledc_handle_t *s_bl;
static uint32_t s_bl_max;

static void set_brightness(uint8_t percent)
{
    if (!s_bl) {
        return;
    }
    uint32_t duty = (uint32_t)percent * s_bl_max / 100;
    ledc_set_duty(s_bl->speed_mode, s_bl->channel, duty);
    ledc_update_duty(s_bl->speed_mode, s_bl->channel);
}

static void backlight_init(void)
{
    if (esp_board_manager_init_device_by_name("lcd_brightness") != ESP_OK) {
        ESP_LOGW(TAG, "taustavalguse juhtimine puudub");
        return;
    }
    dev_ledc_ctrl_config_t *dev_cfg = NULL;
    periph_ledc_config_t *ledc_cfg = NULL;
    if (esp_board_manager_get_device_handle("lcd_brightness", (void **)&s_bl) != ESP_OK ||
        esp_board_manager_get_device_config("lcd_brightness", (void **)&dev_cfg) != ESP_OK ||
        esp_board_periph_get_config(dev_cfg->ledc_name, (void **)&ledc_cfg) != ESP_OK) {
        s_bl = NULL;
        return;
    }
    s_bl_max = (1u << (uint32_t)ledc_cfg->duty_resolution) - 1;
}

/* --- viimane vaade NVS-is ------------------------------------------------ */
static int load_page(void)
{
    nvs_handle_t h;
    int8_t page = 0;
    if (nvs_open("dash", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i8(h, "page", &page);
        nvs_close(h);
    }
    return page;
}

static void save_page(int page)
{
    nvs_handle_t h;
    if (nvs_open("dash", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i8(h, "page", (int8_t)page);
        nvs_commit(h);
        nvs_close(h);
    }
}

/* --- valikuline füüsiline vaate nupp -------------------------------------- */
#if CONFIG_SIMPLDASH_PAGE_BUTTON_GPIO >= 0
static void button_task(void *arg)
{
    const gpio_num_t pin = (gpio_num_t)CONFIG_SIMPLDASH_PAGE_BUTTON_GPIO;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    int stable = 1, count = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        int lvl = gpio_get_level(pin);
        if (lvl != stable) {
            if (++count >= 3) {          /* 30 ms põrkevastane */
                stable = lvl;
                count = 0;
                if (stable == 0) dash_ui_request_page_step(1);
            }
        } else {
            count = 0;
        }
    }
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "SimplDash v%s", SIMPLDASH_VERSION);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Ekraan + puutetundlikkus Guitioni plaaditoe kaudu */
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("display_lcd"));
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("lcd_touch"));
    backlight_init();

    dev_display_lcd_handles_t *lcd = NULL;
    dev_display_lcd_config_t *lcd_cfg = NULL;
    dev_lcd_touch_handles_t *tp = NULL;
    ESP_ERROR_CHECK(esp_board_manager_get_device_handle("display_lcd", (void **)&lcd));
    ESP_ERROR_CHECK(esp_board_manager_get_device_config("display_lcd", (void **)&lcd_cfg));
    ESP_ERROR_CHECK(esp_board_manager_get_device_handle("lcd_touch", (void **)&tp));

    /* LVGL */
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 4;
    port_cfg.task_stack = 16384;
    port_cfg.task_affinity = 1;  /* graafika teisel tuumal, CAN esimesel */
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd->io_handle,
        .panel_handle = lcd->panel_handle,
        .buffer_size = lcd_cfg->lcd_width * lcd_cfg->lcd_height,
        .double_buffer = true,
        .hres = lcd_cfg->lcd_width,
        .vres = lcd_cfg->lcd_height,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_spiram = true,
            .direct_mode = true,
        },
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {
        .flags = { .avoid_tearing = true },  /* joonista otse DSI kaadripuhvritesse */
    };
    lv_display_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "LVGL ekraani lisamine ebaõnnestus");
        return;
    }
    if (tp && tp->touch_handle) {
        const lvgl_port_touch_cfg_t touch_cfg = { .disp = disp, .handle = tp->touch_handle };
        lvgl_port_add_touch(&touch_cfg);
    }

    lvgl_port_lock(0);
    const dash_ui_cfg_t ui_cfg = {
        .on_brightness = set_brightness,
        .on_page = save_page,
        .start_page = load_page(),
    };
    dash_ui_create(&ui_cfg);
    lvgl_port_unlock();
    set_brightness(100);

#if CONFIG_SIMPLDASH_PAGE_BUTTON_GPIO >= 0
    xTaskCreate(button_task, "page_btn", 2048, NULL, 3, NULL);
#endif

    /* Andmeallikad */
    demo_start_task();
#if CONFIG_SIMPLDASH_DEMO_ON_BOOT
    demo_set_enabled(true);
#endif
    gps_start();  /* ainult kui SIMPLDASH_GPS_RX_GPIO >= 0 */
    if (can_rx_start() != ESP_OK) {
        ESP_LOGE(TAG, "CAN ei käivitunud – kontrolli GPIO seadeid (idf.py menuconfig → SimplDash)");
    }
}
