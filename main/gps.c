#include "gps.h"
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static gps_data_t s_gps;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static char s_line[128];
static size_t s_len;

static int hexv(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

/* ddmm.mmmm + N/S/E/W → kraadid */
static double nmea_deg(const char *v, const char *hemi)
{
    if (!*v) return 0;
    double x = atof(v);
    int deg = (int)(x / 100);
    double d = deg + (x - deg * 100) / 60.0;
    return (*hemi == 'S' || *hemi == 'W') ? -d : d;
}

static void parse_line(char *line)
{
    /* kontrollsumma: $....*HH */
    if (line[0] != '$') return;
    char *star = strchr(line, '*');
    if (!star || hexv(star[1]) < 0 || hexv(star[2]) < 0) return;
    uint8_t sum = 0;
    for (char *p = line + 1; p < star; p++) sum ^= (uint8_t)*p;
    if (sum != (uint8_t)(hexv(star[1]) * 16 + hexv(star[2]))) return;
    *star = 0;

    char *f[20];
    int n = 0;
    for (char *p = line + 1; n < 20;) {
        f[n++] = p;
        char *c = strchr(p, ',');
        if (!c) break;
        *c = 0;
        p = c + 1;
    }
    if (strlen(f[0]) < 5) return;
    const char *type = f[0] + strlen(f[0]) - 3;   /* GPRMC, GNRMC, ... */
    int64_t now = esp_timer_get_time();

    if (!strcmp(type, "RMC") && n >= 10) {
        /* 1 aeg, 2 olek A/V, 3-4 lat, 5-6 lon, 7 kiirus sõlmedes, 8 kurss, 9 kuupäev */
        bool ok = f[2][0] == 'A';
        portENTER_CRITICAL(&s_lock);
        s_gps.fix = ok;
        if (ok) {
            s_gps.lat = nmea_deg(f[3], f[4]);
            s_gps.lon = nmea_deg(f[5], f[6]);
            s_gps.speed_kmh = (float)(atof(f[7]) * 1.852);
            if (f[8][0]) s_gps.course = (float)atof(f[8]);
        }
        if (strlen(f[1]) >= 6 && strlen(f[9]) == 6) {
            s_gps.hour = (f[1][0] - '0') * 10 + (f[1][1] - '0');
            s_gps.min = (f[1][2] - '0') * 10 + (f[1][3] - '0');
            s_gps.sec = (float)atof(f[1] + 4);
            s_gps.day = (f[9][0] - '0') * 10 + (f[9][1] - '0');
            s_gps.month = (f[9][2] - '0') * 10 + (f[9][3] - '0');
            s_gps.year = 2000 + (f[9][4] - '0') * 10 + (f[9][5] - '0');
            s_gps.time_valid = s_gps.year >= 2024;
        }
        s_gps.sample_us = now;
        s_gps.samples++;
        s_gps.last_rx_us = now;
        portEXIT_CRITICAL(&s_lock);
    } else if (!strcmp(type, "GGA") && n >= 9) {
        /* 6 kvaliteet, 7 satelliidid, 8 HDOP */
        portENTER_CRITICAL(&s_lock);
        s_gps.sats = atoi(f[7]);
        s_gps.hdop = (float)atof(f[8]);
        s_gps.last_rx_us = now;
        portEXIT_CRITICAL(&s_lock);
    } else {
        portENTER_CRITICAL(&s_lock);
        s_gps.last_rx_us = now;
        portEXIT_CRITICAL(&s_lock);
    }
}

void gps_feed(const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '$') s_len = 0;
        if (c == '\r' || c == '\n') {
            if (s_len) {
                s_line[s_len] = 0;
                parse_line(s_line);
                s_len = 0;
            }
        } else if (s_len < sizeof(s_line) - 1) {
            s_line[s_len++] = c;
        } else {
            s_len = 0;  /* liiga pikk rida – viska ära */
        }
    }
}

void gps_snapshot(gps_data_t *out)
{
    portENTER_CRITICAL(&s_lock);
    *out = s_gps;
    portEXIT_CRITICAL(&s_lock);
    out->connected = out->last_rx_us != 0 &&
                     (esp_timer_get_time() - out->last_rx_us) < (int64_t)GPS_TIMEOUT_MS * 1000;
    if (!out->connected) out->fix = false;
}

void gps_reset(void)
{
    portENTER_CRITICAL(&s_lock);
    memset(&s_gps, 0, sizeof(s_gps));
    portEXIT_CRITICAL(&s_lock);
    s_len = 0;
}

/* --- plaadi UART ------------------------------------------------------------ */
#ifndef DASH_HOST
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define GPS_UART UART_NUM_2

#if CONFIG_SIMPLDASH_GPS_RX_GPIO >= 0
static void gps_task(void *arg)
{
    (void)arg;
    static uint8_t buf[256];
    for (;;) {
        int n = uart_read_bytes(GPS_UART, buf, sizeof(buf), pdMS_TO_TICKS(100));
        if (n > 0) gps_feed((const char *)buf, (size_t)n);
    }
}
#endif

void gps_start(void)
{
#if CONFIG_SIMPLDASH_GPS_RX_GPIO >= 0
    const uart_config_t cfg = {
        .baud_rate = CONFIG_SIMPLDASH_GPS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    if (uart_driver_install(GPS_UART, 4096, 0, 0, NULL, 0) != ESP_OK ||
        uart_param_config(GPS_UART, &cfg) != ESP_OK ||
        uart_set_pin(GPS_UART, CONFIG_SIMPLDASH_GPS_TX_GPIO >= 0 ? CONFIG_SIMPLDASH_GPS_TX_GPIO : UART_PIN_NO_CHANGE,
                     CONFIG_SIMPLDASH_GPS_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE("gps", "UART ei käivitunud");
        return;
    }
    xTaskCreate(gps_task, "gps", 4096, NULL, 5, NULL);
    ESP_LOGI("gps", "GPS: RX GPIO%d, %d baud", CONFIG_SIMPLDASH_GPS_RX_GPIO, CONFIG_SIMPLDASH_GPS_BAUD);
#endif
}
#else
void gps_start(void) {}
#endif
