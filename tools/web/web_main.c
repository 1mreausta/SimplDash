/*
 * SimplDash veebisimulaator (WebAssembly / Emscripten).
 * Sama kasutajaliides (dash_ui.c, page_*.c, maxxecu.c, demo.c) jookseb otse brauseris.
 * JavaScript (index.html) joonistab kaadripuhvri canvas'ele ja saadab hiire/puute sündmused.
 */
#include <emscripten.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "dash_ui.h"
#include "demo.h"
#include "lvgl.h"
#include "maxxecu.h"

#define SW 1024
#define SH 600

int64_t g_fake_time_us;  /* esp_timer_get_time() stubi allikas */

static uint16_t s_fb[SW * SH];   /* viimane valmis kaader (RGB565) */
static uint32_t s_frame_no = 1;
static int s_bright = 100, s_page = 0;
static bool s_can = true;

typedef struct { int16_t x, y; bool pressed; } ev_t;
static ev_t s_q[64];
static int s_qh, s_qt;
static ev_t s_last;

static double s_t0;
static int64_t s_next_demo;

static uint32_t tick_cb(void) { return (uint32_t)(g_fake_time_us / 1000); }

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a; (void)px;
    if (lv_display_flush_is_last(d)) {
        memcpy(s_fb, lv_display_get_buf_active(d)->data, sizeof(s_fb));
        s_frame_no++;
    }
    lv_display_flush_ready(d);
}

static void touch_cb(lv_indev_t *i, lv_indev_data_t *data)
{
    (void)i;
    if (s_qh != s_qt) {
        s_last = s_q[s_qt];
        s_qt = (s_qt + 1) % 64;
        data->continue_reading = s_qh != s_qt;
    }
    data->point.x = s_last.x;
    data->point.y = s_last.y;
    data->state = s_last.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void on_bright(uint8_t pct) { s_bright = pct; }
static void on_page(int p) { s_page = p; }

/* --- JavaScripti jaoks ---------------------------------------------------- */
EMSCRIPTEN_KEEPALIVE void sim_pointer(int x, int y, int pressed)
{
    int nh = (s_qh + 1) % 64;
    if (nh != s_qt) {
        s_q[s_qh] = (ev_t){ (int16_t)x, (int16_t)y, pressed != 0 };
        s_qh = nh;
    }
}

/* 1 järgmine vaade, 2 eelmine, 3 demo, 4 CAN, 5 ülekuumenemine */
EMSCRIPTEN_KEEPALIVE void sim_key(int k)
{
    switch (k) {
    case 1: dash_ui_request_page_step(1); break;
    case 2: dash_ui_request_page_step(-1); break;
    case 3: demo_set_enabled(!demo_is_enabled()); break;
    case 4: s_can = !s_can; break;
    case 5: demo_set_overheat(!demo_is_overheat()); break;
    case 8: demo_set_knock(!demo_is_knock()); break;
    case 9: dash_ui_show_boot(); break;
    case 6: dash_ui_set_speed_source((dash_ui_get_speed_source() + 1) % 3); break;
    case 7: demo_set_gps(!demo_is_gps()); break;
    }
}

/* bitid: 0 demo, 1 CAN, 2 kuum, 3 GPS, 4..7 vaade, 8..15 heledus, 16..17 kiiruse allikas */
EMSCRIPTEN_KEEPALIVE int sim_state(void)
{
    return demo_is_enabled() | (s_can << 1) | (demo_is_overheat() << 2) | (demo_is_gps() << 3) | (s_page << 4) |
           (s_bright << 8) | (dash_ui_get_speed_source() << 16) | (demo_is_knock() << 18);
}

EMSCRIPTEN_KEEPALIVE uint16_t *sim_fb(void) { return s_fb; }

/* Jõudlustest: joonista n täisekraani kaadrit, tagastab keskmise ms kaadri kohta */
EMSCRIPTEN_KEEPALIVE double sim_bench(int n)
{
    double t = emscripten_get_now();
    for (int i = 0; i < n; i++) {
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(NULL);
    }
    return (emscripten_get_now() - t) / n;
}
EMSCRIPTEN_KEEPALIVE uint32_t sim_frame_no(void) { return s_frame_no; }

/* --- põhitsükkel (iga brauseri kaadri ajal) -------------------------------- */
static void loop(void)
{
    g_fake_time_us = 1000000 + (int64_t)((emscripten_get_now() - s_t0) * 1000.0);
    int steps = 0;
    while (g_fake_time_us >= s_next_demo) {
        if (s_can && demo_is_enabled() && steps < 50) demo_step();  /* peidetud tab ei kuhjata */
        s_next_demo += 20000;
        steps++;
    }
    lv_timer_handler();
}

int main(void)
{
    s_t0 = emscripten_get_now();
    g_fake_time_us = 1000000;
    s_next_demo = g_fake_time_us;

    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *d = lv_display_create(SW, SH);
    static uint8_t buf[SW * SH * 2] __attribute__((aligned(64)));
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush_cb);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, touch_cb);

    const dash_ui_cfg_t cfg = { .on_brightness = on_bright, .on_page = on_page, .start_page = 0 };
    dash_ui_create(&cfg);
    demo_set_enabled(true);
    demo_set_gps(true);

    emscripten_set_main_loop(loop, 0, false);
    return 0;
}
