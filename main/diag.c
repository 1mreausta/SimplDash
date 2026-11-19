#include "diag.h"
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static diag_id_t s_ids[DIAG_MAX_IDS];
static uint32_t  s_prev[DIAG_MAX_IDS];
static int       s_n;
static volatile uint32_t s_total, s_busoff, s_overflow, s_other;
static uint32_t  s_total_prev;
static float     s_total_hz;
static int64_t   s_last_tick;

static char      s_gps[DIAG_GPS_LINES][100];
static int       s_gps_head, s_gps_n;
static volatile uint32_t s_gps_good, s_gps_bad;

void diag_can_rx(uint32_t id, uint8_t dlc)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_lock);
    s_total++;
    int i;
    for (i = 0; i < s_n && s_ids[i].id != id; i++) {}
    if (i == s_n) {
        if (s_n < DIAG_MAX_IDS) {
            /* sorteeritult sisse */
            int k = s_n;
            while (k > 0 && s_ids[k - 1].id > id) {
                s_ids[k] = s_ids[k - 1];
                s_prev[k] = s_prev[k - 1];
                k--;
            }
            memset(&s_ids[k], 0, sizeof(s_ids[k]));
            s_ids[k].id = (uint16_t)id;
            s_prev[k] = 0;
            s_n++;
            i = k;
        } else {
            s_other++;
            i = -1;
        }
    }
    if (i >= 0) {
        s_ids[i].count++;
        s_ids[i].dlc = dlc;
        s_ids[i].last_us = now;
    }
    portEXIT_CRITICAL(&s_lock);
}

static struct { int state, tec, rec; uint32_t bus_err; bool lo, set; } s_cs;

void diag_can_state(int state, int tec, int rec, uint32_t bus_errors, bool listen_only)
{
    s_cs.state = state;
    s_cs.tec = tec;
    s_cs.rec = rec;
    s_cs.bus_err = bus_errors;
    s_cs.lo = listen_only;
    s_cs.set = true;
}

void diag_can_state_text(char *buf, size_t cap)
{
    static const char *ST[] = { "aktiivne (OK)", "HOIATUS", "PASSIIVNE", "BUS-OFF" };
    if (!s_cs.set) { buf[0] = 0; return; }
    if (s_cs.state < 0) { snprintf(buf, cap, "CAN ei käivitunud – kontrolli pinne (menuconfig)"); return; }
    snprintf(buf, cap, "kontroller %s  ·  vead TX %d / RX %d  ·  siini vigu %u%s", ST[s_cs.state & 3], s_cs.tec,
             s_cs.rec, (unsigned)s_cs.bus_err, s_cs.lo ? "  ·  listen-only" : "");
}

void diag_can_busoff(void) { s_busoff++; }
void diag_can_overflow(void) { s_overflow++; }

void diag_gps_line(const char *line, bool ok)
{
    portENTER_CRITICAL(&s_lock);
    snprintf(s_gps[s_gps_head], sizeof(s_gps[0]), "%s%s", ok ? "" : "[VIGA] ", line);
    s_gps_head = (s_gps_head + 1) % DIAG_GPS_LINES;
    if (s_gps_n < DIAG_GPS_LINES) s_gps_n++;
    portEXIT_CRITICAL(&s_lock);
    if (ok) s_gps_good++;
    else s_gps_bad++;
}

void diag_tick(int64_t now)
{
    if (!s_last_tick) {
        s_last_tick = now;
        return;
    }
    float dt = (now - s_last_tick) / 1e6f;
    if (dt < 1.0f) return;
    s_last_tick = now;
    portENTER_CRITICAL(&s_lock);
    for (int i = 0; i < s_n; i++) {
        s_ids[i].hz = (s_ids[i].count - s_prev[i]) / dt;
        s_prev[i] = s_ids[i].count;
    }
    s_total_hz = (s_total - s_total_prev) / dt;
    s_total_prev = s_total;
    portEXIT_CRITICAL(&s_lock);
}

int diag_can_ids(diag_id_t *out, int max)
{
    portENTER_CRITICAL(&s_lock);
    int n = s_n < max ? s_n : max;
    memcpy(out, s_ids, sizeof(diag_id_t) * (size_t)n);
    portEXIT_CRITICAL(&s_lock);
    return n;
}

uint32_t diag_can_total(void) { return s_total; }
uint32_t diag_can_busoffs(void) { return s_busoff; }
uint32_t diag_can_overflows(void) { return s_overflow; }
float diag_can_total_hz(void) { return s_total_hz; }
uint32_t diag_gps_good(void) { return s_gps_good; }
uint32_t diag_gps_bad(void) { return s_gps_bad; }

int diag_gps_lines(char out[][100], int max)
{
    portENTER_CRITICAL(&s_lock);
    int n = s_gps_n < max ? s_gps_n : max;
    for (int i = 0; i < n; i++) {
        int k = ((s_gps_head - 1 - i) % DIAG_GPS_LINES + DIAG_GPS_LINES) % DIAG_GPS_LINES;
        memcpy(out[i], s_gps[k], 100);
    }
    portEXIT_CRITICAL(&s_lock);
    return n;
}

#ifndef DASH_HOST
#include "esp_heap_caps.h"
#include "esp_system.h"

void diag_system(char *buf, size_t cap)
{
    static const char *R[] = { "teadmata", "toide sisse", "väline", "tarkvara", "viga (panic)", "katkestuse valvur",
                               "tööülesande valvur", "valvur", "unest", "toitepinge langus", "SDIO", "USB", "JTAG",
                               "eFuse", "toite rike", "CPU lukk" };
    int r = (int)esp_reset_reason();
    uint32_t up = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(buf, cap, "töötab %u:%02u:%02u  ·  vaba mälu %u kB (sisemine %u kB, PSRAM %u kB)  ·  käivitus: %s",
             (unsigned)(up / 3600), (unsigned)(up / 60 % 60), (unsigned)(up % 60),
             (unsigned)(esp_get_free_heap_size() / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
             r >= 0 && r < (int)(sizeof(R) / sizeof(R[0])) ? R[r] : "?");
}
#else
void diag_system(char *buf, size_t cap)
{
    uint32_t up = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(buf, cap, "simulaator  ·  töötab %u:%02u:%02u", (unsigned)(up / 3600), (unsigned)(up / 60 % 60),
             (unsigned)(up % 60));
}
#endif
