/*
 * SimplDash kasutajaliides (LVGL 9), 1024x600 – tuum.
 * Vaated on failides page_*.c. Siin: vaadete vahetus, puudutused, hoiatused ja
 * ühised kihid (hoiatusriba, punane raam, "ECU ühendus puudub", DEMO märk).
 */
#include "dash_ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alarms.h"
#include "can_ecu.h"
#include "dash_common.h"
#include "demo.h"
#include "diag.h"
#include "lamps.h"
#include "logger.h"
#include "settings.h"
#include "stats.h"
#include "esp_timer.h"
#include "version.h"

static const dash_page_t *const PAGES[] = { &page_race, &page_gauges, &page_sweep, &page_street,
                                             &page_classic, &page_accel, &page_lap, &page_track,
                                             &page_data, &page_settings };
#define N_PAGES ((int)(sizeof(PAGES) / sizeof(PAGES[0])))
_Static_assert(N_PAGES <= 16, "vaadete peitmise bitimask on 16-bitine");

static const uint8_t BRIGHT[] = DASH_BRIGHTNESS_LEVELS;

/* Libistamise lävi (px) ja puudutuse lubatud liikumine */
#define SWIPE_MIN_PX  90
#define TAP_MAX_PX    30

static struct {
    dash_ui_cfg_t cfg;
    lv_obj_t *page_obj[N_PAGES];
    int       page;
    int       pending_step;
    lv_obj_t *warn_box, *warn_lbl, *alarm_frame;
    char      warn_txt[96];
    lv_obj_t *nocomm, *demo_badge;
    lv_obj_t *alert, *alert_lbl;     /* täisekraani hoiatus */
    char      alert_txt[48];
    int8_t    alert_col;
    lv_obj_t *toast, *toast_lbl, *toast_dots[N_PAGES];
    int64_t   toast_until_us;
    int       bright_idx;
    int64_t   press_start_us;
    lv_point_t press_pt;
    int       rev_limit;
    lv_obj_t *fade;          /* must kiht vaadete vahetamise sujuvaks üleminekuks */
    int       fade_target;   /* -1 = üleminekut pole käimas */
    bool      fade_dark;     /* true = vaade on vahetatud, nüüd heledamaks */
    lv_obj_t *boot, *boot_content, *boot_dots[15];   /* käivitusekraan */
    int8_t    boot_lit;
    bool      booting;
    /* GPS */
    lv_obj_t *gps_box, *gps_lbl, *clock_lbl;
    char      gps_txt[16], clock_txt[8];
    int8_t    gps_col;
    bool      gps_seen;
    bool      avail[N_PAGES];
    int64_t   start_us;
} ui;

extern const lv_image_dsc_t boot_logo;  /* boot_logo.c (tools/logo/make_boot_logo.py) */

/* ---------------------------------------------------------------------- */
static void show_toast(void)
{
    lv_label_set_text(ui.toast_lbl, PAGES[ui.page]->name);
    for (int i = 0; i < N_PAGES; i++) {
        lv_obj_set_hidden(ui.toast_dots[i], !ui.avail[i] && i != ui.page);
        lv_obj_set_style_bg_color(ui.toast_dots[i], i == ui.page ? C_ACCENT : lv_color_hex(0x3A414B), 0);
    }
    lv_obj_set_hidden(ui.toast, false);
    ui.toast_until_us = esp_timer_get_time() + 1200000;
}

static void open_page(int page, bool notify)
{
    page = ((page % N_PAGES) + N_PAGES) % N_PAGES;
    for (int i = 0; i < N_PAGES; i++) lv_obj_set_hidden(ui.page_obj[i], i != page);
    bool changed = page != ui.page;
    ui.page = page;
    lv_obj_align(ui.warn_box, LV_ALIGN_TOP_MID, 0, PAGES[page]->warn_y);
    if (ui.gps_box) {
        lv_obj_set_pos(ui.gps_box, PAGES[page]->gps_x, PAGES[page]->gps_y);
        lv_obj_set_pos(ui.clock_lbl, PAGES[page]->clock_x, PAGES[page]->clock_y);
        lv_obj_set_pos(ui.demo_badge, PAGES[page]->demo_x, PAGES[page]->demo_y);
    }
    if (changed && notify) {
        show_toast();
        if (ui.cfg.on_page) ui.cfg.on_page(page);
    }
}

/* --- üleminek: vana vaade tumeneb mustaks, vahetus, uus vaade heleneb ---------------- */
static void fade_exec(void *obj, int32_t v)
{
    lv_obj_set_style_bg_opa(obj, (lv_opa_t)v, 0);
}

static void fade_start(int32_t from, int32_t to);

static void fade_done(lv_anim_t *a)
{
    (void)a;
    if (!ui.fade_dark) {
        /* täiesti must: vaheta vaade ja hakka helenema */
        open_page(ui.fade_target, true);
        ui.fade_dark = true;
        fade_start(LV_OPA_COVER, LV_OPA_TRANSP);
    } else {
        lv_obj_set_hidden(ui.fade, true);
        ui.fade_target = -1;
    }
}

static void fade_start(int32_t from, int32_t to)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui.fade);
    lv_anim_set_exec_cb(&a, fade_exec);
    lv_anim_set_values(&a, from, to);
    int32_t ms = DASH_PAGE_FADE_MS / 2 * (from > to ? from - to : to - from) / LV_OPA_COVER;
    lv_anim_set_duration(&a, ms > 0 ? ms : 1);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, fade_done);
    lv_anim_start(&a);
}

/* vaade, kuhu parajasti minnakse (ka ülemineku ajal) */
static int target_page(void)
{
    return ui.fade_target >= 0 ? ui.fade_target : ui.page;
}

/* --- käivitusekraan: logo ilmub (sama üleminek nagu vaadetel), shift-light'id
 *     süttivad järjest, logo kaob ja dash heleneb nähtavale ------------------------ */
static void boot_exec(void *obj, int32_t t)
{
    (void)obj;
    const int32_t half = DASH_PAGE_FADE_MS / 2 > 0 ? DASH_PAGE_FADE_MS / 2 : 1;
    /* sujuv algus ja lõpp (ease in-out), sama kestus nagu vaadete üleminekul */
    int32_t edge = t < half ? t : (t > DASH_BOOT_MS - half ? DASH_BOOT_MS - t : half);
    float p = (float)edge / half;
    p = p < 0 ? 0 : p > 1 ? 1 : p;
    p = p < 0.5f ? 2 * p * p : 1 - 2 * (1 - p) * (1 - p);
    int32_t opa = (int32_t)(p * LV_OPA_COVER);
    lv_obj_set_style_opa(ui.boot_content, (lv_opa_t)opa, 0);

    /* tuled süttivad ühtlaselt; kõik põlevad enne kadumist */
    int32_t span = DASH_BOOT_MS - 3 * half;
    int lit = span > 0 ? (int)((t - half) * 15 / span) : 15;
    lit = lit < 0 ? 0 : lit > 15 ? 15 : lit;
    for (int i = ui.boot_lit; i < lit; i++) {
        lv_obj_set_style_bg_color(ui.boot_dots[i], i < 6 ? C_GREEN : i < 11 ? C_YELLOW : C_RED, 0);
    }
    if (lit > ui.boot_lit) ui.boot_lit = (int8_t)lit;
}

static void fade_start(int32_t from, int32_t to);

static void boot_done(lv_anim_t *a)
{
    (void)a;
    lv_obj_delete(ui.boot);
    ui.boot = NULL;
    ui.booting = false;
    /* dash heleneb mustast nähtavale, täpselt nagu vaate vahetusel */
    if (DASH_PAGE_FADE_MS > 0) {
        lv_obj_set_style_bg_opa(ui.fade, LV_OPA_COVER, 0);
        lv_obj_set_hidden(ui.fade, false);
        ui.fade_target = ui.page;
        ui.fade_dark = true;
        fade_start(LV_OPA_COVER, LV_OPA_TRANSP);
    }
}

static void boot_create(lv_obj_t *scr)
{
    ui.boot = dash_box(scr, 0, 0, W, H, C_BG, 0);
    ui.boot_content = dash_box(ui.boot, 0, 0, 680, 230, C_BG, 0);
    lv_obj_set_style_bg_opa(ui.boot_content, LV_OPA_TRANSP, 0);
    lv_obj_center(ui.boot_content);
    lv_obj_set_style_opa(ui.boot_content, LV_OPA_TRANSP, 0);

    lv_obj_t *img = lv_image_create(ui.boot_content);
    lv_image_set_src(img, &boot_logo);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_clickable(img, false);

    const int dw = 30, dg = 8, n = 15;
    int x0 = (680 - (n * dw + (n - 1) * dg)) / 2;
    for (int i = 0; i < n; i++) {
        ui.boot_dots[i] = dash_box(ui.boot_content, x0 + i * (dw + dg), 156, dw, 9, C_OFF, 4);
    }
    lv_obj_t *sub = dash_label(ui.boot_content, 20, C_DIM, "MaxxECU RACE DASH  ·  v" SIMPLDASH_VERSION);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 186);
    ui.boot_lit = 0;
    ui.booting = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui.boot);
    lv_anim_set_exec_cb(&a, boot_exec);
    lv_anim_set_values(&a, 0, DASH_BOOT_MS);
    lv_anim_set_duration(&a, DASH_BOOT_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_completed_cb(&a, boot_done);
    lv_anim_start(&a);
}

void dash_ui_show_boot(void)
{
    if (ui.booting || DASH_BOOT_MS <= 0) return;
    boot_create(lv_screen_active());
}

void dash_ui_set_page(int page)
{
    page = ((page % N_PAGES) + N_PAGES) % N_PAGES;
    if (DASH_PAGE_FADE_MS <= 0 || !ui.fade) {
        open_page(page, true);
        return;
    }
    if (page == target_page()) return;
    ui.fade_target = page;
    /* jätka praegusest heledusest (kui üleminek juba käib, ei hüppa) */
    int32_t cur = lv_obj_is_hidden(ui.fade) ? LV_OPA_TRANSP : lv_obj_get_style_bg_opa(ui.fade, 0);
    lv_anim_delete(ui.fade, fade_exec);
    lv_obj_set_hidden(ui.fade, false);
    ui.fade_dark = false;
    fade_start(cur, LV_OPA_COVER);
}

void dash_ui_request_page_step(int dir)
{
    __atomic_fetch_add(&ui.pending_step, dir, __ATOMIC_RELAXED);  /* ui_tick tarbib */
}

int dash_ui_page_count(void)
{
    return N_PAGES;
}

/* järgmine saadaval vaade suunas dir (GPS-vaated jäävad vahele, kui GPS-i pole) */
static int page_from(int from, int dir)
{
    int p = from;
    for (int i = 0; i < N_PAGES; i++) {
        p = ((p + dir) % N_PAGES + N_PAGES) % N_PAGES;
        if (ui.avail[p]) return p;
    }
    return from;
}

static void page_step(int steps)
{
    int p = target_page();
    for (int i = 0; i < abs(steps) && i < N_PAGES; i++) p = page_from(p, steps > 0 ? 1 : -1);
    dash_ui_set_page(p);
}

void dash_ui_set_speed_source(int src)
{
    g_set.speed_src = (int8_t)src;
    settings_changed();
}

int dash_ui_get_speed_source(void)
{
    return g_set.speed_src;
}

bool dash_ui_local_time(const gps_data_t *g, int *hh, int *mm);

/* --- kell: GPS UTC → kohalik aeg (EL suveaeg) ----------------------------- */
static int64_t days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static int64_t last_sunday_utc(int y, int m)   /* kuu viimane pühapäev 01:00 UTC, sekundites */
{
    int64_t d = days_from_civil(y, m + 1, 1) - 1;  /* kuu viimane päev (m = 3 või 10) */
    int wd = (int)((d % 7 + 11) % 7);  /* 0 = pühapäev (1970-01-01 oli neljapäev) */
    return (d - wd) * 86400 + 3600;
}

bool dash_ui_local_time(const gps_data_t *g, int *hh, int *mm)
{
    if (!g->time_valid) return false;
    int64_t t = days_from_civil(g->year, g->month, g->day) * 86400 + g->hour * 3600 + g->min * 60 + (int)g->sec;
    int64_t off = (int64_t)g_set.tz_offset_min * 60;
    if (g_set.tz_dst && t >= last_sunday_utc(g->year, 3) && t < last_sunday_utc(g->year, 10)) off += 3600;
    t += off;
    int64_t sod = ((t % 86400) + 86400) % 86400;
    *hh = (int)(sod / 3600);
    *mm = (int)(sod / 60 % 60);
    return true;
}

/* ---------------------------------------------------------------------- */
static void on_screen_event(lv_event_t *e)
{
    if (ui.booting) return;  /* käivitusekraani ajal puudutusi ei arvesta */
    lv_event_code_t c = lv_event_get_code(e);
    /* puudutus (heledus) ja pikk vajutus (demo) ainult tühjal taustal – mitte nuppudel/loenditel;
     * libistamine töötab kõikjal (loendid annavad sündmuse edasi) */
    bool on_bg = lv_event_get_target_obj(e) == lv_event_get_current_target_obj(e);
    lv_indev_t *indev = lv_indev_active();
    lv_point_t pt = { 0, 0 };
    if (indev) lv_indev_get_point(indev, &pt);

    if (c == LV_EVENT_PRESSED) {
        ui.press_start_us = esp_timer_get_time();
        ui.press_pt = pt;
    } else if (c == LV_EVENT_RELEASED) {
        int64_t ms = (esp_timer_get_time() - ui.press_start_us) / 1000;
        int dx = pt.x - ui.press_pt.x, dy = pt.y - ui.press_pt.y;
        if (abs(dx) >= SWIPE_MIN_PX && abs(dx) > abs(dy)) {
            /* libistamine: vasakule = järgmine vaade */
            page_step(dx < 0 ? 1 : -1);
        } else if (abs(dx) > TAP_MAX_PX || abs(dy) > TAP_MAX_PX) {
            /* pooleli jäänud libistus – ei tee midagi */
        } else if (!on_bg) {
            /* nupp / loend – ei midagi */
        } else if (ms >= 2500) {
            /* pikk vajutus: demorežiim sisse/välja */
            demo_set_enabled(!demo_is_enabled());
        } else if (ms < 600 && ui.cfg.on_brightness) {
            /* lühike puudutus: heledus */
            ui.bright_idx = (ui.bright_idx + 1) % (int)sizeof(BRIGHT);
            ui.cfg.on_brightness(BRIGHT[ui.bright_idx]);
        }
    }
}

static dash_state_t s_state;

const struct dash_state_s *dash_ui_state(void)
{
    return &s_state;
}

static void ui_tick(lv_timer_t *tm)
{
    (void)tm;
    static dash_state_t s;
    maxxecu_snapshot(&s.d);
    int64_t now = esp_timer_get_time();
    s.stale = s.d.frame_count == 0 || (now - s.d.last_rx_us) > (int64_t)DASH_COMM_TIMEOUT_MS * 1000;
    s.blink = (now / 250000) % 2;
    s.blink_fast = (now / 80000) % 2;
    s.rpm = s.d.valid[SIG_RPM] ? s.d.v[SIG_RPM] : 0;
    s.flashing = s.rpm >= g_set.shift_flash;
    if (s.d.valid[SIG_REVLIMIT_RPM] && s.d.v[SIG_REVLIMIT_RPM] > 1000) {
        ui.rev_limit = (int)s.d.v[SIG_REVLIMIT_RPM];
    }
    s.rev_limit = ui.rev_limit;
    s.lamps = lamps_get();
    s.now_us = now;

    /* GPS ja kiiruse allikas */
    gps_snapshot(&s.gps);
    s.gps_ok = s.gps.connected;
    if (s.gps_ok) ui.gps_seen = true;
    s.speed_src = g_set.speed_src;
    s.speed_gps = s.speed_src == DASH_SPEED_GPS || (s.speed_src == DASH_SPEED_AUTO && s.gps.fix);
    if (s.speed_gps) {
        s.d.v[SIG_SPEED] = s.gps.fix && s.gps.speed_kmh >= DASH_GPS_MIN_KMH ? s.gps.speed_kmh : 0;
        s.d.valid[SIG_SPEED] = s.gps.fix;
    }
    for (int i = 0; i < N_PAGES; i++) {
        ui.avail[i] = (!PAGES[i]->available || PAGES[i]->available(&s)) &&
                      (!((g_set.pages_off >> i) & 1) || PAGES[i] == &page_settings);   /* SEADED ei peitu */
    }
    /* GPS-vaade, aga GPS kadus (või ei tulnud 15 s jooksul pärast käivitust) → esimene vaade */
    if (!ui.booting && !ui.avail[target_page()] &&
        (ui.gps_seen || now - ui.start_us > 15000000)) {
        dash_ui_set_page(0);
    }

    /* GPS ikoon: roheline = asukoht olemas, kollane = otsib satelliite */
    lv_obj_set_hidden(ui.gps_box, !s.gps_ok || PAGES[ui.page]->gps_x < 0);   /* < 0 = vaade näitab ise */
    lv_obj_set_hidden(ui.clock_lbl, !(s.gps_ok && s.gps.time_valid) || PAGES[ui.page]->clock_x < 0);
    if (s.gps_ok) {
        char g[16];
        if (s.gps.fix) snprintf(g, sizeof(g), "GPS %d", s.gps.sats);
        else snprintf(g, sizeof(g), "GPS …");
        dash_set_text_if(ui.gps_lbl, ui.gps_txt, sizeof(ui.gps_txt), g);
        int8_t col = s.gps.fix ? (s.speed_gps ? 2 : 1) : 0;
        if (col != ui.gps_col) {
            lv_color_t c = col == 2 ? C_GREEN : col == 1 ? lv_color_hex(0x8FE3B0) : C_YELLOW;
            lv_obj_set_style_text_color(ui.gps_lbl, c, 0);
            lv_obj_set_style_border_color(ui.gps_box, c, 0);
            ui.gps_col = col;
        }
        int hh, mm;
        if (dash_ui_local_time(&s.gps, &hh, &mm)) {
            char c[8];
            snprintf(c, sizeof(c), "%02d:%02d", hh, mm);
            dash_set_text_if(ui.clock_lbl, ui.clock_txt, sizeof(ui.clock_txt), c);
        }
    }

    /* nupust tulnud vaatevahetus */
    int step = ui.booting ? 0 : __atomic_exchange_n(&ui.pending_step, 0, __ATOMIC_RELAXED);
    if (step) page_step(step);
    if (ui.toast_until_us && now > ui.toast_until_us) {
        lv_obj_set_hidden(ui.toast, true);
        ui.toast_until_us = 0;
    }

    /* SEADED vaates (seaded, diagnostika) ei kata "ECU ühendus puudub" ekraani */
    lv_obj_set_hidden(ui.nocomm, !s.stale || PAGES[ui.page] == &page_settings);
    lv_obj_set_hidden(ui.demo_badge, !demo_is_enabled() || PAGES[ui.page]->demo_x < 0);

    /* hoiatused */
    alarms_eval(&s);
    if (s.n_warn) dash_set_text_if(ui.warn_lbl, ui.warn_txt, sizeof(ui.warn_txt), s.warn_txt);
    lv_obj_set_hidden(ui.warn_box, !(s.n_warn && s.blink));
    lv_obj_set_hidden(ui.alarm_frame, !(s.n_warn && s.blink));
    /* täisekraani hoiatus: vilgub punane / tumepunane, puudutus = kinnita (60 s vaikust) */
    lv_obj_set_hidden(ui.alert, !s.alert_full || ui.booting);
    if (s.alert_full) {
        dash_set_text_if(ui.alert_lbl, ui.alert_txt, sizeof(ui.alert_txt), s.alert_txt);
        int8_t col = s.blink;
        if (col != ui.alert_col) {
            lv_obj_set_style_bg_color(ui.alert, col ? C_RED : lv_color_hex(0x5A0000), 0);
            ui.alert_col = col;
        }
    }

    diag_tick(now);
    stats_update(&s);
    logger_update(&s);
    can_ecu_update(&s);
    settings_tick(now);

    for (int i = 0; i < N_PAGES; i++) {
        if (PAGES[i]->background) PAGES[i]->background(&s);
    }
    PAGES[ui.page]->update(&s);
    s_state = s;   /* veebilehe jaoks (loetakse LVGL lukuga) */
}

/* ---------------------------------------------------------------------- */
static void on_alert(lv_event_t *e)
{
    (void)e;
    alarms_ack();
}

void dash_ui_create(const dash_ui_cfg_t *cfg)
{
    memset(&ui, 0, sizeof(ui));
    static const char *names[N_PAGES];
    for (int i = 0; i < N_PAGES; i++) names[i] = PAGES[i]->name;
    settings_set_pages(names, N_PAGES);
    settings_init();
    stats_init();
    alarms_init();
    if (cfg) ui.cfg = *cfg;
    ui.rev_limit = DASH_RPM_MAX - 500;
    ui.start_us = esp_timer_get_time();
    ui.gps_col = -1;
    for (int i = 0; i < N_PAGES; i++) ui.avail[i] = !PAGES[i]->available;

    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);
    lv_obj_add_event_cb(scr, on_screen_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, on_screen_event, LV_EVENT_RELEASED, NULL);

    for (int i = 0; i < N_PAGES; i++) {
        lv_obj_t *o = dash_box(scr, 0, 0, W, H, C_BG, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
        lv_obj_set_event_bubble(o, true);   /* nuppudelt/loenditelt libistamine jõuab ekraanini */
        PAGES[i]->create(o);
        lv_obj_set_hidden(o, true);
        ui.page_obj[i] = o;
    }

    /* Üleminek: must kiht vaadete peal, hoiatused jäävad selle peale nähtavaks */
    ui.fade_target = -1;
    ui.fade = dash_box(scr, 0, 0, W, H, C_BG, 0);
    lv_obj_set_style_bg_opa(ui.fade, LV_OPA_TRANSP, 0);
    lv_obj_set_hidden(ui.fade, true);

    /* Hoiatuse raam + ribateade */
    ui.alarm_frame = dash_box(scr, 0, 0, W, H, C_BG, 4);
    lv_obj_set_style_bg_opa(ui.alarm_frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ui.alarm_frame, C_RED, 0);
    lv_obj_set_style_border_width(ui.alarm_frame, 7, 0);
    lv_obj_set_hidden(ui.alarm_frame, true);

    ui.warn_box = dash_box(scr, 0, 0, 10, 10, C_RED, 12);
    lv_obj_set_size(ui.warn_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(ui.warn_box, 30, 0);
    lv_obj_set_style_pad_ver(ui.warn_box, 2, 0);
    ui.warn_lbl = dash_label(ui.warn_box, 44, C_TEXT, "");
    lv_obj_center(ui.warn_lbl);
    lv_obj_set_hidden(ui.warn_box, true);

    /* Demo märk */
    ui.demo_badge = dash_box(scr, W - 92, 4, 74, 26, C_YELLOW, 6);
    lv_obj_t *dl = dash_label(ui.demo_badge, 16, C_BG, "DEMO");
    lv_obj_center(dl);
    lv_obj_set_hidden(ui.demo_badge, true);

    /* GPS ikoon + kell (asukoht sõltub vaatest, vt dash_page_t) */
    ui.gps_box = dash_box(scr, 0, 0, 74, 26, C_PANEL, 6);
    lv_obj_set_style_border_width(ui.gps_box, 2, 0);
    lv_obj_set_style_border_color(ui.gps_box, C_YELLOW, 0);
    ui.gps_lbl = dash_label(ui.gps_box, 17, C_YELLOW, "GPS");
    lv_obj_center(ui.gps_lbl);
    lv_obj_set_hidden(ui.gps_box, true);
    ui.clock_lbl = dash_label(scr, 26, C_TEXT, "--:--");
    lv_obj_set_hidden(ui.clock_lbl, true);

    /* Täisekraani hoiatus (puudutus = kinnita) */
    ui.alert = dash_box(scr, 40, 60, W - 80, H - 120, C_RED, 24);
    lv_obj_set_style_border_color(ui.alert, C_TEXT, 0);
    lv_obj_set_style_border_width(ui.alert, 6, 0);
    lv_obj_set_clickable(ui.alert, true);
    lv_obj_add_event_cb(ui.alert, on_alert, LV_EVENT_CLICKED, NULL);
    ui.alert_lbl = dash_label(ui.alert, 120, C_TEXT, "");
    lv_obj_align(ui.alert_lbl, LV_ALIGN_CENTER, 0, -30);
    lv_obj_t *al2 = dash_label(ui.alert, 26, C_TEXT, "puuduta = kinnita (60 s)");
    lv_obj_align(al2, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_set_hidden(ui.alert, true);
    ui.alert_col = -1;

    /* Ühendus puudub */
    ui.nocomm = dash_box(scr, 0, 0, W, H, C_BG, 0);
    lv_obj_set_style_bg_opa(ui.nocomm, LV_OPA_80, 0);
    lv_obj_t *nc1 = dash_label(ui.nocomm, 44, C_YELLOW, "ECU ÜHENDUS PUUDUB");
    lv_obj_align(nc1, LV_ALIGN_CENTER, 0, -20);
    lv_obj_t *nc2 = dash_label(ui.nocomm, 17, C_DIM,
                               "ootan CAN andmeid…   hoia ekraani 3 s = demorežiim  ·  libista = vaade");
    lv_obj_align(nc2, LV_ALIGN_CENTER, 0, 30);

    /* Vaate nimi + täpid (näidatakse vahetamisel 1,2 s), kõige peal */
    ui.toast = dash_box(scr, 0, 0, 10, 10, lv_color_hex(0x0E1116), 14);
    lv_obj_set_size(ui.toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_border_color(ui.toast, lv_color_hex(0x2B3139), 0);
    lv_obj_set_style_border_width(ui.toast, 1, 0);
    lv_obj_set_style_pad_hor(ui.toast, 28, 0);
    lv_obj_set_style_pad_ver(ui.toast, 10, 0);
    lv_obj_set_flex_flow(ui.toast, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.toast, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui.toast, 8, 0);
    ui.toast_lbl = dash_label(ui.toast, 40, C_TEXT, "");
    lv_obj_t *dots = lv_obj_create(ui.toast);
    lv_obj_remove_style_all(dots);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dots, 10, 0);
    lv_obj_set_scrollable(dots, false);
    lv_obj_set_clickable(dots, false);
    for (int i = 0; i < N_PAGES; i++) ui.toast_dots[i] = dash_box(dots, 0, 0, 12, 12, C_DIM, 6);
    lv_obj_align(ui.toast, LV_ALIGN_BOTTOM_MID, 0, -110);
    lv_obj_set_hidden(ui.toast, true);

    ui.page = -1;
    open_page(ui.cfg.start_page >= 0 && ui.cfg.start_page < N_PAGES ? ui.cfg.start_page : 0, false);

    if (DASH_BOOT_MS > 0) boot_create(scr);  /* kõige peal */

    lv_timer_create(ui_tick, DASH_UI_PERIOD_MS, NULL);
}
