/*
 * Vaade ANALOOG: 90ndate Jaapani sportauto stiilis osutitega kellad halli paneeli peal.
 *   vasakul: vee temperatuur (C–H), hetke kütusekulu (L/100km, seistes L/h)
 *   keskel:  spidomeeter (läbimõõdiku aknas kiirus) ja tahhomeeter (kollane/punane tsoon, aknas käik)
 *   paremal: õlirõhk (bar)
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dash_common.h"
#include "demo.h"
#include "needles.h"
#include "stats.h"

#define C_FACE    lv_color_hex(0xF3F4EF)
#define C_INK     lv_color_hex(0x15171A)
#define C_NEEDLE  lv_color_hex(0xFF3B1C)
#define C_BEZEL   lv_color_hex(0x9AA0A8)
#define C_RINGB   lv_color_hex(0x0E0F11)
#define C_ZONE_Y  lv_color_hex(0xF2C230)
#define C_ZONE_R  lv_color_hex(0xF04A3A)
#define SMALL_CY  448   /* väikeste kellade keskpunkti kõrgus (kõik samal joonel) */

typedef struct {
    lv_obj_t *face, *scale, *needle, *unit, *lcd, *lcd_txt;
    int32_t   min, max, last, rot, sweep;
    char      t_lcd[12], t_unit[16];
    int8_t    rim;
} cg_t;

static struct {
    cg_t clt, fuel, spd, tach, oil;
    lv_obj_t *zone_y, *zone_r;
    int rev_limit;
    lv_obj_t *ind_demo, *ind_gps;     /* mustad märgutuled spidomeetris */
    int8_t    demo_on, gps_on;
    dash_lamps_t lamps;               /* tänavarežiim: suunatuled, kaugtuled */
    lv_obj_t *odo, *odo_txt;          /* tänavarežiim: läbisõidu aken spidomeetris */
    char      t_odo[20];
    int8_t    odo_shown;
} p;

static const char *TACH_TXT[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", NULL };

/* Skaalade geomeetria nagu originaalis (0° = kell 3, päripäeva):
 *   spido: 0 vasakul (180°), 0,75° / km/h  → 120 üleval, 240 paremal, 320 all paremal
 *   tahho: 0 all (90°), 25,8° / 1000 rpm   → 7 peaaegu üleval, 11 paremal veidi all */
#define SPD_ROT    180
#define SPD_SWEEP  (DASH_CLASSIC_SPEED_MAX * 3 / 4)
#define TACH_ROT   90
#define TACH_SWEEP (DASH_CLASSIC_RPM_MAX * 258 / 10000)
/* spido: suured numbrid iga 40 järel (väikesed 20, 60, … eraldi skaalaga) */
static const char *SPD_BIG_TXT[] = { "0", "", "40", "", "80", "", "120", "", "160", "", "200", "", "240", "",
                                     "280", "", "320", "", "360", NULL };
static const char *CLT_TXT[]  = { "C", "", "H", NULL };

/* rot/sweep: skaala algnurk ja ulatus (0° = kell 3, päripäeva).
 * pdx: osuti telje nihe keskelt paremale (0 = keskel); psr: skaala raadius teljest (0 = vaikimisi) */
static void cg_create(cg_t *g, lv_obj_t *scr, int cx, int cy, int r, int min, int max, int ticks, int major,
                      int rot, int sweep, const char **txt, int label_px, const char *unit, int lcd_w,
                      int pdx, int psr)
{
    memset(g, 0, sizeof(*g));
    g->min = min;
    g->max = max;
    g->rot = rot;
    g->sweep = sweep;
    g->last = INT32_MIN;
    g->rim = -1;
    const bool big = r > 120;

    /* hõbedane äär + must rõngas + valge sihverplaat */
    const int bz = big ? 7 : 5;
    lv_obj_t *bezel = dash_box(scr, cx - r - bz, cy - r - bz, 2 * (r + bz), 2 * (r + bz), C_BEZEL, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_grad_color(bezel, lv_color_hex(0x4A4F57), 0);
    lv_obj_set_style_bg_grad_dir(bezel, LV_GRAD_DIR_VER, 0);
    g->face = dash_box(scr, cx - r, cy - r, 2 * r, 2 * r, C_FACE, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_color(g->face, C_RINGB, 0);
    lv_obj_set_style_border_width(g->face, big ? 9 : 6, 0);

    lv_obj_t *s = lv_scale_create(scr);
    const int sr = psr ? psr : r - (big ? 14 : 9);
    const int px = cx + pdx;   /* osuti telg */
    lv_obj_set_size(s, 2 * sr, 2 * sr);
    lv_obj_set_pos(s, px - sr, cy - sr);
    lv_scale_set_mode(s, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(s, min, max);
    lv_scale_set_total_tick_count(s, ticks);
    lv_scale_set_major_tick_every(s, major);
    lv_scale_set_label_show(s, true);
    if (txt) lv_scale_set_text_src(s, txt);
    lv_scale_set_angle_range(s, sweep);
    lv_scale_set_rotation(s, rot);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_set_style_arc_width(s, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_length(s, big ? 18 : 10, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(s, big ? 5 : 3, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(s, C_INK, LV_PART_INDICATOR);   /* kõigil kelladel mustad */
    lv_obj_set_style_text_color(s, C_INK, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(s, dash_font(label_px), LV_PART_INDICATOR);
    lv_obj_set_style_pad_radial(s, big ? 6 : 3, LV_PART_INDICATOR);
    lv_obj_set_style_length(s, big ? 9 : 6, LV_PART_ITEMS);
    lv_obj_set_style_line_width(s, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(s, C_INK, LV_PART_ITEMS);
    lv_obj_set_clickable(s, false);
    lv_obj_set_scrollable(s, false);
    g->scale = s;

    /* ühik / tekst sihverplaadil (osuti alla) */
    g->unit = dash_label(scr, big ? 20 : 12, C_INK, unit);
    if (pdx) {   /* telg paremal: ühik otse telje alla – "0" on vasakul all, osuti ei käi siin */
        lv_obj_set_width(g->unit, 60);
        lv_obj_set_style_text_align(g->unit, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(g->unit, px - 1 - 30, cy + r * 36 / 100);
    } else {
        lv_obj_set_width(g->unit, r * 2 - 40);
        lv_obj_set_style_text_align(g->unit, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(g->unit, cx - r + 20, cy + (big ? r * 14 / 100 : r * 22 / 100));
    }

    /* "läbimõõdiku" aken numbriga */
    if (lcd_w) {
        g->lcd = dash_box(scr, cx - lcd_w / 2, cy + r * 33 / 100, lcd_w, 44, lv_color_hex(0x0B0C0E), 6);
        lv_obj_set_style_border_color(g->lcd, lv_color_hex(0x3A3F46), 0);
        lv_obj_set_style_border_width(g->lcd, 2, 0);
        g->lcd_txt = dash_label(g->lcd, 34, lv_color_hex(0xE9ECEF), "0");
        lv_obj_center(g->lcd_txt);
    }

    /* kese: tume kuppel (osuti on selle peal, nagu originaalis) */
    const int hub = big ? 20 : r * 30 / 100;
    lv_obj_t *h = dash_box(scr, px - hub, cy - hub, 2 * hub, 2 * hub, lv_color_hex(0x3A3E44), LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_grad_color(h, lv_color_hex(0x08090A), 0);
    lv_obj_set_style_bg_grad_dir(h, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(h, lv_color_hex(0x1A1C1F), 0);
    lv_obj_set_style_border_width(h, 1, 0);
    lv_obj_set_style_shadow_color(h, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_width(h, big ? 10 : 6, 0);
    lv_obj_set_style_shadow_offset_y(h, 2, 0);
    lv_obj_set_style_shadow_opa(h, LV_OPA_40, 0);

    /* osuti: pilt (koonus + saba, 3D varjund), pöörleb ümber telje */
    const lv_image_dsc_t *img = big ? &needle_big : &needle_small;
    const int pvx = big ? NEEDLE_BIG_PIVOT_X : NEEDLE_SMALL_PIVOT_X;
    g->needle = lv_image_create(scr);
    lv_image_set_src(g->needle, img);
    lv_image_set_pivot(g->needle, pvx, img->header.h / 2);
    lv_obj_set_pos(g->needle, px - pvx, cy - img->header.h / 2);
    lv_image_set_rotation(g->needle, rot * 10);
    lv_obj_set_clickable(g->needle, false);
}

static void cg_set(cg_t *g, float v, int32_t quant)
{
    int32_t iv = (int32_t)lroundf(v);
    if (quant > 1) iv = iv / quant * quant;
    iv = iv < g->min ? g->min : iv > g->max ? g->max : iv;
    if (iv != g->last) {
        int32_t a10 = g->rot * 10 + (int32_t)((int64_t)g->sweep * 10 * (iv - g->min) / (g->max - g->min));
        lv_image_set_rotation(g->needle, a10);
        g->last = iv;
    }
}

static void cg_lcd(cg_t *g, const char *txt)
{
    if (g->lcd_txt) dash_set_text_if(g->lcd_txt, g->t_lcd, sizeof(g->t_lcd), txt);
}

/* sihverplaadi siseäär: 0 must, 1 punane (hoiatus), 2 sinine (käiguvahetus) */
static void cg_rim(cg_t *g, int8_t state)
{
    if (state == g->rim) return;
    lv_obj_set_style_border_color(g->face, state == 1 ? C_RED : state == 2 ? C_BLUE : C_RINGB, 0);
    g->rim = state;
}

/* tahhomeetri kollane/punane tsoon väliservas – sihverplaadi laps, seega kriipsude ja osuti all */
static lv_obj_t *zone_arc(lv_obj_t *face, int face_r, int r, lv_color_t c)
{
    lv_obj_t *a = lv_arc_create(face);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, 2 * r, 2 * r);
    const int bw = lv_obj_get_style_border_width(face, 0);   /* lapse koordinaadid algavad äärise seest */
    lv_obj_set_pos(a, face_r - r - bw, face_r - r - bw);
    lv_arc_set_rotation(a, TACH_ROT);
    lv_obj_set_style_arc_width(a, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, c, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_clickable(a, false);
    return a;
}

static void set_zones(int limit)
{
    int y0 = g_set.shift_start < limit ? g_set.shift_start : limit;
    int32_t ay = (int32_t)((float)TACH_SWEEP * y0 / DASH_CLASSIC_RPM_MAX);
    int32_t ar = (int32_t)((float)TACH_SWEEP * limit / DASH_CLASSIC_RPM_MAX);
    lv_arc_set_bg_angles(p.zone_y, ay, ar > ay ? ar : ay + 1);
    lv_arc_set_bg_angles(p.zone_r, ar, TACH_SWEEP);
}

/* spidomeetri väikesed numbrid (20, 60, 100, …): eraldi skaala ilma kriipsudeta, samal raadiusel */
static void small_labels(lv_obj_t *scr, int cx, int cy, int r)
{
    lv_obj_t *s = lv_scale_create(scr);
    const int sr = r - 14;
    lv_obj_set_size(s, 2 * sr, 2 * sr);
    lv_obj_set_pos(s, cx - sr, cy - sr);
    lv_scale_set_mode(s, LV_SCALE_MODE_ROUND_INNER);
    const int first = 20, n = (DASH_CLASSIC_SPEED_MAX - first) / 40 + 1, last = first + (n - 1) * 40;
    lv_scale_set_range(s, first, last);
    lv_scale_set_total_tick_count(s, n);
    lv_scale_set_major_tick_every(s, 1);
    lv_scale_set_label_show(s, true);
    lv_scale_set_rotation(s, SPD_ROT + first * 3 / 4);
    lv_scale_set_angle_range(s, (last - first) * 3 / 4);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_set_style_arc_opa(s, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_length(s, 18, LV_PART_INDICATOR);           /* sama kaugus nagu suurtel */
    lv_obj_set_style_line_opa(s, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(s, C_INK, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(s, dash_font(16), LV_PART_INDICATOR);
    lv_obj_set_style_pad_radial(s, 4, LV_PART_INDICATOR);
    lv_obj_set_clickable(s, false);
}

/* must aken tekstiga (märgutuli): väljas peaaegu nähtamatu */
#define C_IND_OFF lv_color_hex(0x24272B)
#define IND_W 50
#define HUB_R   20   /* suure kella kupli raadius (vt cg_create) */
#define HUB_GAP 22   /* ühiku teksti ja LCD kaugus kuplist */
#define UNIT_DESC 5  /* rea kõrguse tühi osa teksti all (et nähtav vahe oleks sama) */
static lv_obj_t *indicator(lv_obj_t *scr, int x, int y, const char *txt)
{
    lv_obj_t *b = dash_box(scr, x, y, IND_W, 24, lv_color_hex(0x0B0C0E), 4);
    lv_obj_set_style_border_color(b, lv_color_hex(0x3A3F46), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_t *l = dash_label(b, 14, C_IND_OFF, txt);
    lv_obj_center(l);
    return b;
}

static void indicator_set(lv_obj_t *b, int8_t *cache, bool on, lv_color_t col)
{
    if (on == *cache) return;
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), on ? col : C_IND_OFF, 0);
    *cache = on;
}

static void create(lv_obj_t *scr)
{
    /* must paneel peene äärega */
    lv_obj_t *panel = dash_box(scr, 6, 74, W - 12, 486, C_BG, 40);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x2A2D32), 0);
    lv_obj_set_style_border_width(panel, 2, 0);

    /* suured: spidomeeter ja tahhomeeter */
    const int bcy = 304, br = 178;
    cg_create(&p.spd, scr, 410, bcy, br, 0, DASH_CLASSIC_SPEED_MAX, DASH_CLASSIC_SPEED_MAX / 10 + 1, 2,
              SPD_ROT, SPD_SWEEP, SPD_BIG_TXT, 30, "km/h", 112, 0, br - 9);
    /* kriipsud välisäärde (musta rõnga vastu) ja lühemad, numbrid jäävad samale kohale */
    lv_obj_set_style_length(p.spd.scale, 13, LV_PART_INDICATOR);
    lv_obj_set_style_length(p.spd.scale, 7, LV_PART_ITEMS);
    lv_obj_set_style_line_width(p.spd.scale, 4, LV_PART_INDICATOR);
    lv_obj_set_style_pad_radial(p.spd.scale, 16, LV_PART_INDICATOR);
    small_labels(scr, 410, bcy, br);
    /* "km/h" telje kohale, kiiruse aken telje alla – mõlemad sama kaugel kuplist (HUB_GAP) */
    const int uh = lv_font_get_line_height(dash_font(20));
    lv_obj_set_width(p.spd.lcd, 66);
    lv_obj_set_pos(p.spd.lcd, 410 - 33, bcy + HUB_R + HUB_GAP);
    lv_obj_set_pos(p.spd.unit, 410 - (br * 2 - 40) / 2, bcy - HUB_R - HUB_GAP - uh + UNIT_DESC);
    /* spidomeetri all kaks musta märgutuld: DEMO (kollane) ja GPS (roheline) */
    /* keskel osuti telje all ("320" jääb paremale üles) */
    p.ind_demo = indicator(scr, 410 - 3 - IND_W, bcy + 132, "DEMO");
    p.ind_gps = indicator(scr, 410 + 3, bcy + 132, "GPS");
    p.demo_on = p.gps_on = -1;
    /* tänavarežiim: läbisõit all kahe suure kella vahel (seal, kus äärised lahknevad) */
    p.odo = dash_box(scr, 595 - 62, bcy + 168, 124, 32, lv_color_hex(0x0B0C0E), 5);
    lv_obj_set_style_border_color(p.odo, lv_color_hex(0x3A3F46), 0);
    lv_obj_set_style_border_width(p.odo, 2, 0);
    p.odo_txt = dash_label(p.odo, 22, lv_color_hex(0xE9ECEF), "");
    lv_obj_center(p.odo_txt);
    lv_obj_set_hidden(p.odo, true);

    cg_create(&p.tach, scr, 780, bcy, br, 0, DASH_CLASSIC_RPM_MAX, DASH_CLASSIC_RPM_MAX / 250 + 1, 4,
              TACH_ROT, TACH_SWEEP, TACH_TXT, 40, "x1000 r/min", 64, 0, 0);
    /* samamoodi: "x1000 r/min" telje kohale, käigu aken telje alla */
    lv_obj_set_width(p.tach.lcd, 50);
    lv_obj_set_pos(p.tach.lcd, 780 - 25, bcy + HUB_R + HUB_GAP);
    lv_obj_set_width(p.tach.unit, 130);
    lv_obj_set_pos(p.tach.unit, 780 - 65, bcy - HUB_R - HUB_GAP - uh + UNIT_DESC);
    p.zone_y = zone_arc(p.tach.face, br, br - 9, C_ZONE_Y);
    p.zone_r = zone_arc(p.tach.face, br, br - 9, C_ZONE_R);
    p.rev_limit = DASH_RPM_MAX - 500;
    set_zones(p.rev_limit);

    /* väikesed: vesi, kütusekulu (vasakul), õlirõhk (paremal) – suurte peal */
    /* vesi ja kütusekulu: sama suured, kõrvuti samal kõrgusel */
    const int sr = 68, scy = SMALL_CY, sgap = 4;
    /* väikesed: osuti telg paremal, skaala vasakul veerandkaarel (all min, üleval max) */
#define SMALL(r) (r) * 38 / 100, (r) * 105 / 100
    cg_create(&p.clt, scr, 80, scy, sr, 40, 130, 7, 3, 135, 90, CLT_TXT, 18, "°C", 0, SMALL(sr));
    cg_create(&p.fuel, scr, 80 + 2 * (sr + 5) + sgap, scy, sr, 0, DASH_FUEL_GAUGE_MAX, 7, 3, 135, 90, NULL, 16,
              "L/100km", 0, SMALL(sr));
    cg_create(&p.oil, scr, W - 10 - (sr + 5), SMALL_CY, sr, 0, DASH_OIL_GAUGE_MAX, DASH_OIL_GAUGE_MAX + 1, 4, 135, 90,
              NULL, 16, "bar", 0, SMALL(sr));
    /* tänavarežiim: nooled ja kaugtuled üleval kahe suure kella vahel */
    dash_lamps_create(&p.lamps, scr, 595 - 75 - 22, 595 + 75 - 22, 595 - 22, 82, 34);
}

static void update(const dash_state_t *s)
{
    char buf[16];
    if (s->rev_limit != p.rev_limit) {
        p.rev_limit = s->rev_limit;
        set_zones(p.rev_limit);
    }

    float spd = s->d.valid[SIG_SPEED] ? s->d.v[SIG_SPEED] : 0;
    cg_set(&p.spd, spd, 1);
    snprintf(buf, sizeof(buf), "%d", (int)(spd + 0.5f));
    cg_lcd(&p.spd, buf);
    indicator_set(p.ind_demo, &p.demo_on, demo_is_enabled(), C_YELLOW);
    indicator_set(p.ind_gps, &p.gps_on, s->gps_ok, C_GREEN);
    dash_lamps_update(&p.lamps, s);
    if (p.odo_shown != (g_set.road_mode != 0)) {
        p.odo_shown = g_set.road_mode != 0;
        lv_obj_set_hidden(p.odo, !p.odo_shown);
    }
    if (p.odo_shown) {
        char o[20];
        dash_fmt_odo(o, sizeof(o));
        dash_set_text_if(p.odo_txt, p.t_odo, sizeof(p.t_odo), o);
    }

    cg_set(&p.tach, s->rpm, 20);
    dash_gear_text(s, buf, sizeof(buf));
    cg_lcd(&p.tach, buf);
    cg_rim(&p.tach, s->flashing && s->blink_fast ? 2 : 0);

    /* kütusekulu pihustite töötsüklist (RB25DET: 6 × 630 cc) */
    bool per_km;
    float fuel = stats_fuel_now(s, &per_km);
    cg_set(&p.fuel, fuel >= 0 ? fuel : 0, 1);
    if (fuel >= 0) dash_set_text_if(p.fuel.unit, p.fuel.t_unit, sizeof(p.fuel.t_unit), per_km ? "L/100km" : "L/h");

    cg_set(&p.clt, s->d.valid[SIG_CLT] ? s->d.v[SIG_CLT] : 40, 1);
    cg_rim(&p.clt, dash_sig_alarm(s, SIG_CLT) && s->blink ? 1 : 0);

    float oil = s->d.valid[SIG_OIL_PRESS] ? s->d.v[SIG_OIL_PRESS] * 0.01f : 0;   /* kPa → bar */
    cg_set(&p.oil, oil, 1);
    cg_rim(&p.oil, dash_sig_alarm(s, SIG_OIL_PRESS) && s->blink ? 1 : 0);
}

const dash_page_t page_classic = { "ANALOOG", 8, create, update, NULL, NULL, -1, 0, -1, 0, -1, 0 };
