/*
 * SimplDash testid arvutis (sama kood nagu plaadil): dekooder, õlirõhu kõver, ringi joon,
 * veebilehe seadete sisend, sildi teksti vahemälu.   tools/test/run.sh
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "alarms.h"
#include "dash_common.h"
#include "lap_core.h"
#include "gps_pages.h"
#include "maxxecu.h"
#include "settings.h"
#include "stats.h"
#include "store.h"
#include <stdlib.h>
#include <unistd.h>

int64_t g_fake_time_us = 1000000;
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define NEAR(a, b, e) CHECK(fabsf((float)(a) - (float)(b)) <= (e))

static void test_decoder(void)
{
    /* 0x520: rpm 6500, TPS 45.6 %, MAP 180.5 kPa, lambda 0.812 (little-endian int16) */
    uint8_t d[8] = { 0x64, 0x19, 0xC8, 0x01, 0x0D, 0x07, 0x2C, 0x03 };
    CHECK(maxxecu_decode(0x520, d, 8));
    ecu_data_t e;
    maxxecu_snapshot(&e);
    NEAR(e.v[SIG_RPM], 6500, 0.5f);
    NEAR(e.v[SIG_TPS], 45.6f, 0.01f);
    NEAR(e.v[SIG_MAP], 180.5f, 0.01f);
    NEAR(e.v[SIG_LAMBDA], 0.812f, 0.0005f);
    /* negatiivne väärtus (märgiga int16): IAT −12.3 °C kaadris 0x530 baidid 4..5 */
    float v[SIG_COUNT] = { 0 };
    v[SIG_IAT] = -12.3f;
    v[SIG_BATT] = 13.85f;
    uint8_t o[8];
    CHECK(maxxecu_encode(0x530, v, 0, o));
    CHECK(maxxecu_decode(0x530, o, 8));
    maxxecu_snapshot(&e);
    NEAR(e.v[SIG_IAT], -12.3f, 0.06f);
    NEAR(e.v[SIG_BATT], 13.85f, 0.006f);
    /* lühike või võõras kaader ei muuda midagi */
    CHECK(!maxxecu_decode(0x520, d, 4));
    CHECK(!maxxecu_decode(0x123, d, 8));
}

static void test_oil_curve(void)
{
    /* vaikekõver: 800→70, 2000→120, 4000→200, 6000→250 kPa */
    NEAR(alarms_oil_min(500), 70, 0.01f);     /* alla esimese punkti: konstant */
    NEAR(alarms_oil_min(800), 70, 0.01f);
    NEAR(alarms_oil_min(1400), 95, 0.01f);    /* poolel teel 70 → 120 */
    NEAR(alarms_oil_min(3000), 160, 0.01f);
    NEAR(alarms_oil_min(9000), 250, 0.01f);   /* üle viimase: konstant */
}

static void gps_at(dash_state_t *s, double y_m, uint32_t n)
{
    s->gps.fix = true;
    s->gps.lat = 59.0 + y_m / 110540.0;
    s->gps.lon = 24.0;
    s->gps.speed_kmh = 100;
    s->gps.course = 0;
    s->gps.samples = n;
    s->gps.sample_us = n * 100000LL;   /* 10 Hz */
}

static void test_lap_line(void)
{
    static dash_state_t s;
    lap_set_line(59.0, 24.0, 0, false);   /* joon põhja suunas, ring algab ületamisel */
    uint32_t n = 1;
    for (double y = -45; y <= 45; y += 10) { gps_at(&s, y, n++); lap_sample(&s); }
    CHECK(lap_info()->running);
    /* joon ületati −5 m (mõõtmine 5) ja +5 m (mõõtmine 6) vahel → aeg interpoleeritakse poole peale */
    CHECK(lap_info()->lap_start == 5 * 100000LL + 50000);
    /* tagasi lõunasse ületades (vale suund) ring ei lõpe */
    for (double y = 45; y >= -45; y -= 10) { gps_at(&s, y, n++); lap_sample(&s); }
    CHECK(lap_info()->done == 0);
}

static void test_settings_input(void)
{
    const set_item_t *it;
    int n = settings_items(&it);
    const set_item_t *shift = NULL, *pass = NULL, *w0 = NULL;
    for (int i = 0; i < n; i++) {
        if (!strcmp(it[i].key, "shift_flash")) shift = &it[i];
        if (!strcmp(it[i].key, "wifi_pass")) pass = &it[i];
        if (!strcmp(it[i].key, "w0_hi")) w0 = &it[i];
    }
    CHECK(shift && pass && w0);
    CHECK(!settings_item_set(shift, "abc"));                 /* mitte-arv */
    CHECK(settings_item_set(shift, "99999") && g_set.shift_flash == 12000);   /* piiratakse */
    CHECK(settings_item_set(shift, "1000") && g_set.shift_start < g_set.shift_flash);   /* start nihkub alla */
    const set_item_t *start = NULL;
    for (int i = 0; i < n; i++) if (!strcmp(it[i].key, "shift_start")) start = &it[i];
    CHECK(start && settings_item_set(start, "11900") && g_set.shift_flash == 12000);   /* flash nihkub kaasa, mitte üle piiri */
    CHECK(settings_item_set(start, "99999") && g_set.shift_start == 11900 && g_set.shift_flash == 12000);
    char big[100];
    memset(big, 'x', sizeof(big) - 1);
    big[99] = 0;
    CHECK(!settings_item_set(pass, big));                    /* liiga pikk tekst */
    CHECK(settings_item_set(w0, "") && isnan(g_set.warn_high[0]));   /* tühi = hoiatus väljas */
    CHECK(settings_item_set(w0, "104") && fabsf(g_set.warn_high[0] - 104) < 0.01f);
    settings_reset();
}

static void test_text_cache(void)
{
    lv_obj_t *l = lv_label_create(lv_screen_active());
    char c[16] = "";
    dash_set_text_if(l, c, sizeof(c), "12.5");
    CHECK(!strcmp(lv_label_get_text(l), "12.5"));
    const char *a = "abcdefghijklmnopqrstuvwxyz-1", *b = "abcdefghijklmnopqrstuvwxyz-2";
    dash_set_text_if(l, c, sizeof(c), a);
    CHECK(!strcmp(lv_label_get_text(l), a));
    lv_label_set_text(l, "MUUDETUD");          /* kui sama pikk tekst tuleb uuesti, ei joonistata */
    dash_set_text_if(l, c, sizeof(c), a);
    CHECK(!strcmp(lv_label_get_text(l), "MUUDETUD"));
    dash_set_text_if(l, c, sizeof(c), b);      /* muutus pika teksti lõpus jõuab ekraanile */
    CHECK(!strcmp(lv_label_get_text(l), b));
}

static void test_store_grow(void)
{
    char dir[] = "/tmp/simpldash_testXXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    setenv("SIMPLDASH_DATA", dir, 1);
    /* vanem (lühem) plokk loetakse algusesse, lõpp jääb vaikeväärtuseks */
    const uint8_t old[4] = { 1, 2, 3, 4 };
    CHECK(store_save("t", old, sizeof(old)));
    uint8_t buf[8];
    memset(buf, 0xAA, sizeof(buf));
    CHECK(store_load_grow("t", buf, sizeof(buf)));
    CHECK(buf[0] == 1 && buf[3] == 4 && buf[4] == 0xAA && buf[7] == 0xAA);
    CHECK(!store_load("t", buf, sizeof(buf)));            /* täpne lugemine nõuab sama suurust */
    uint8_t small[2];
    CHECK(!store_load_grow("t", small, sizeof(small)));   /* uuem (pikem) plokk vanemasse versiooni ei sobi */
    /* seaded: salvestatud väärtus jääb alles, salvestis on ilma lõpu täiteta */
    g_set.shift_start = 5000;
    g_set.shift_flash = 7000;
    CHECK(store_save("settings", &g_set, SET_DATA_END));
    settings_reset();
    settings_init();
    CHECK(g_set.shift_start == 5000 && g_set.shift_flash == 7000);
    settings_reset();
    store_erase("t");
    store_erase("settings");
    rmdir(dir);
    unsetenv("SIMPLDASH_DATA");
}

static uint32_t tick(void) { return (uint32_t)(g_fake_time_us / 1000); }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) { (void)a; (void)p; lv_display_flush_ready(d); }

int main(void)
{
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t *d = lv_display_create(64, 64);
    static uint8_t buf[64 * 64 * 2];
    lv_display_set_buffers(d, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush);
    settings_init();

    test_decoder();
    test_oil_curve();
    test_lap_line();
    test_settings_input();
    test_text_cache();
    test_store_grow();
    puts(fails ? "TESTID EBAÕNNESTUSID" : "kõik testid OK");
    return fails != 0;
}
