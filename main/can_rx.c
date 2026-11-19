#include "can_rx.h"
#include <string.h>
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_timer.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "diag.h"
#include "maxxecu.h"
#include "sdkconfig.h"

static const char *TAG = "can_rx";

typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} rx_item_t;

static QueueHandle_t s_queue;
static volatile uint32_t s_ovf;   /* ISR: järjekord täis */
static twai_node_handle_t s_node;

/* ISR: loe kaader ja pane järjekorda (ujukomaarvutus käib tavalises taskis) */
static IRAM_ATTR bool on_rx(twai_node_handle_t h, const twai_rx_done_event_data_t *e, void *ctx)
{
    uint8_t buf[8];
    twai_frame_t f = { .buffer = buf, .buffer_len = sizeof(buf) };
    BaseType_t woken = pdFALSE;
    if (twai_node_receive_from_isr(h, &f) == ESP_OK && !f.header.ide && !f.header.rtr) {
        rx_item_t it = { .id = f.header.id, .dlc = (uint8_t)f.header.dlc };
        memcpy(it.data, buf, 8);
        if (xQueueSendFromISR(s_queue, &it, &woken) != pdTRUE) s_ovf++;
    }
    return woken == pdTRUE;
}

static IRAM_ATTR bool on_state(twai_node_handle_t h, const twai_state_change_event_data_t *e, void *ctx)
{
    /* Bus-off (nt lühis / vale bitikiirus) – taastame automaatselt taskist */
    if (e->new_sta == TWAI_ERROR_BUS_OFF) {
        BaseType_t woken = pdFALSE;
        rx_item_t it = { .id = 0xFFFFFFFF };
        xQueueSendFromISR(s_queue, &it, &woken);
        return woken == pdTRUE;
    }
    return false;
}

static void rx_task(void *arg)
{
    rx_item_t it;
    uint32_t ovf_seen = 0;
    int64_t next_info = 0;
    for (;;) {
        bool got = xQueueReceive(s_queue, &it, pdMS_TO_TICKS(500)) == pdTRUE;
        /* diagnostika: kontrolleri olek ~2 korda sekundis, kadunud kaadrid */
        int64_t now = esp_timer_get_time();
        if (now >= next_info) {
            next_info = now + 500000;
            twai_node_status_t st;
            twai_node_record_t rec;
#if CONFIG_SIMPLDASH_CAN_LISTEN_ONLY
            const bool lo = true;
#else
            const bool lo = false;
#endif
            if (twai_node_get_info(s_node, &st, &rec) == ESP_OK)
                diag_can_state((int)st.state, st.tx_error_count, st.rx_error_count, rec.bus_err_num, lo);
        }
        while (ovf_seen != s_ovf) {
            ovf_seen++;
            diag_can_overflow();
        }
        if (!got) {
            continue;
        }
        if (it.id == 0xFFFFFFFF) {
            diag_can_busoff();
            ESP_LOGW(TAG, "CAN bus-off, taastan…");
            vTaskDelay(pdMS_TO_TICKS(100));
            twai_node_recover(s_node);
            continue;
        }
        maxxecu_decode(it.id, it.data, it.dlc);
    }
}

esp_err_t can_rx_start(void)
{
    s_queue = xQueueCreate(128, sizeof(rx_item_t));
    if (!s_queue) {
        return ESP_ERR_NO_MEM;
    }

    twai_onchip_node_config_t cfg = {
        .io_cfg = {
            .tx = CONFIG_SIMPLDASH_CAN_TX_GPIO,
            .rx = CONFIG_SIMPLDASH_CAN_RX_GPIO,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing.bitrate = CONFIG_SIMPLDASH_CAN_BITRATE,
        .tx_queue_depth = 4,
#if CONFIG_SIMPLDASH_CAN_LISTEN_ONLY
        .flags.enable_listen_only = 1,
#endif
    };
    esp_err_t err = twai_new_node_onchip(&cfg, &s_node);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TWAI init ebaõnnestus: %s", esp_err_to_name(err));
        diag_can_state(-1, 0, 0, 0, false);
        return err;
    }

    twai_event_callbacks_t cbs = { .on_rx_done = on_rx, .on_state_change = on_state };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(s_node, &cbs, NULL));
    ESP_ERROR_CHECK(twai_node_enable(s_node));

    xTaskCreate(rx_task, "can_rx", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "CAN käivitatud: TX=GPIO%d RX=GPIO%d %d bit/s%s",
             CONFIG_SIMPLDASH_CAN_TX_GPIO, CONFIG_SIMPLDASH_CAN_RX_GPIO, CONFIG_SIMPLDASH_CAN_BITRATE,
#if CONFIG_SIMPLDASH_CAN_LISTEN_ONLY
             " (listen-only)");
#else
             "");
#endif
    return ESP_OK;
}

bool can_send(uint32_t id, const uint8_t *data, uint8_t len)
{
#if CONFIG_SIMPLDASH_CAN_LISTEN_ONLY
    (void)id; (void)data; (void)len;
    return false;
#else
    if (!s_node || len > 8) return false;
    /* järjekord täis (nt keegi ei kinnita kaadreid) → ära kirjuta üle puhvrit, mida draiver veel saadab */
    twai_node_status_t st;
    if (twai_node_get_info(s_node, &st, NULL) != ESP_OK || st.tx_queue_remaining == 0) return false;
    static uint8_t buf[5][8];          /* järjekord (4) + parasjagu saatmisel olev kaader */
    static int slot;
    slot = (slot + 1) % 5;
    memcpy(buf[slot], data, len);
    twai_frame_t f = { .header = { .id = id, .dlc = len }, .buffer = buf[slot], .buffer_len = len };
    return twai_node_transmit(s_node, &f, 0) == ESP_OK;
#endif
}
