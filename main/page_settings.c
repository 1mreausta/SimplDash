/*
 * Vaade SEADED: seaded (± nupud), sõit (läbisõit, mootoritunnid, reis), hoiatuste logi ja info
 * (GPS, SD-kaart, Wi-Fi, versioon). Libistamine vahetab vaadet ka siin.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "alarms.h"
#include "can_ecu.h"
#include "dash_common.h"
#include "demo.h"
#include "diag.h"
#include "logger.h"
#include "stats.h"
#include "version.h"
#include "wifi_web.h"

enum { TAB_ECU, TAB_SET, TAB_TRIP, TAB_LOG, TAB_INFO, TAB_DIAG, N_TABS };
static const char *TAB_TXT[N_TABS] = { "ECU", "SEADED", "SÕIT", "HOIATUSED", "INFO", "DIAG" };

#define MAX_ROWS 72

static struct {
    int       tab;
    lv_obj_t *tabs[N_TABS], *view[N_TABS];
    /* seaded */
    const set_item_t *items;
    int       n_items;
    lv_obj_t *val[MAX_ROWS];
    char      t_val[MAX_ROWS][40];
    lv_obj_t *note;
    bool      restart_note;
    lv_obj_t *list, *heads[16], *jump[16];
    int       n_heads;
    /* sõit */
    lv_obj_t *trip[8];
    char      t_trip[8][32];
    /* logi */
    lv_obj_t *log_l;
    char      t_log[2048];
    int       log_n;
    /* info */
    lv_obj_t *info_l;
    char      t_info[600];
    /* diagnostika */
    lv_obj_t *dg_can, *dg_cs, *dg_col[4], *dg_gps, *dg_gpsl, *dg_sd, *dg_sys;
    char      t_dg_can[120], t_dg_cs[120], t_dg_col[4][400], t_dg_gps[120], t_dg_gpsl[560], t_dg_sd[120],
              t_dg_sys[200];
    int64_t   dg_next;
} p;

/* libistamine nuppudel/loendil jõuab ekraanini (vaate vahetus) */
static void bubble(lv_obj_t *o)
{
    lv_obj_set_event_bubble(o, true);
}

/* dash_button, aga heledam taust ja suurem kiri */
static lv_obj_t *button(lv_obj_t *parent, const char *txt, int w, int h, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = dash_button(parent, txt, 0, 0, w, h, cb, ud);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x151A21), 0);
    lv_obj_set_style_text_font(lv_obj_get_child(b, 0), dash_font(26), 0);
    return b;
}

static void show_tab(int t)
{
    p.tab = t;
    for (int i = 0; i < N_TABS; i++) {
        lv_obj_set_hidden(p.view[i], i != t);
        lv_obj_set_style_border_color(p.tabs[i], i == t ? C_ACCENT : C_LINE, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(p.tabs[i], 0), i == t ? C_TEXT : C_DIM, 0);
    }
}

static void on_tab(lv_event_t *e) { show_tab((int)(intptr_t)lv_event_get_user_data(e)); }

static void on_step(lv_event_t *e)
{
    intptr_t v = (intptr_t)lv_event_get_user_data(e);
    int idx = (int)(v >> 1), dir = (v & 1) ? 1 : -1;
    const set_item_t *it = &p.items[idx];
    settings_item_step(it, dir);
    if (it->restart) p.restart_note = true;
}

/* jaotise kiirnupp: keri loend selle pealkirjani */
static void on_jump(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_update_layout(p.list);
    int32_t y = lv_obj_get_y(p.heads[k]);
    int32_t max = lv_obj_get_scroll_y(p.list) + lv_obj_get_scroll_bottom(p.list);
    lv_obj_scroll_to_y(p.list, y < max ? y : max, LV_ANIM_ON);
    for (int i = 0; i < p.n_heads; i++) {
        lv_obj_set_style_border_color(p.jump[i], i == k ? C_ACCENT : C_LINE, 0);
    }
}

static void on_trip_reset(lv_event_t *e) { (void)e; stats_trip_reset(); }
static void on_log_clear(lv_event_t *e) { (void)e; alarms_log_clear(); p.log_n = -1; }
static void on_demo(lv_event_t *e) { (void)e; demo_set_enabled(!demo_is_enabled()); }

/* vaikeseaded: esimene vajutus küsib kinnitust, teine (3 s jooksul) taastab */
static int64_t s_def_ask_us, s_def_msg_until;
static lv_obj_t *s_def_lbl;
static int64_t now_us_cached;
static void on_defaults(lv_event_t *e)
{
    (void)e;
    if (s_def_ask_us && now_us_cached - s_def_ask_us < 3000000) {
        settings_reset();
        s_def_ask_us = 0;
        p.restart_note = true;
        lv_label_set_text(s_def_lbl, "TAASTATUD");
        s_def_msg_until = now_us_cached + 2000000;
    } else {
        s_def_ask_us = now_us_cached;
        lv_label_set_text(s_def_lbl, "VAJUTA UUESTI");
    }
}

static lv_obj_t *view_box(lv_obj_t *scr)
{
    lv_obj_t *v = dash_box(scr, 0, 70, W, H - 70, C_BG, 0);
    lv_obj_set_style_bg_opa(v, LV_OPA_TRANSP, 0);
    bubble(v);
    return v;
}

static void create_settings(lv_obj_t *v)
{
    lv_obj_t *list = lv_obj_create(v);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, W - 24, H - 70 - 46 - 50);
    lv_obj_set_pos(list, 12, 50);
    p.list = list;
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_bg_color(list, C_ACCENT, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_50, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(list, 6, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_style_pad_bottom(list, 20, 0);
    bubble(list);

    p.n_items = settings_items(&p.items);
    if (p.n_items > MAX_ROWS) p.n_items = MAX_ROWS;
    for (int i = 0; i < p.n_items; i++) {
        const set_item_t *it = &p.items[i];
        if (it->type == ST_HEAD) {
            lv_obj_t *h = dash_label(list, 22, C_ACCENT, it->label);
            lv_obj_set_style_pad_top(h, i ? 14 : 0, 0);
            if (p.n_heads < 16) p.heads[p.n_heads++] = h;
            continue;
        }
        lv_obj_t *row = dash_box(list, 0, 0, W - 40, 58, C_PANEL, 8);
        lv_obj_set_style_border_color(row, C_LINE, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        bubble(row);
        lv_obj_set_clickable(row, true);
        lv_obj_t *l = dash_label(row, 26, C_TEXT, it->label);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 16, 0);
        p.val[i] = dash_label(row, 28, C_TEXT, "");
        lv_obj_set_width(p.val[i], 260);
        lv_obj_set_style_text_align(p.val[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(p.val[i], LV_ALIGN_RIGHT_MID, -86, 0);
        if (it->type == ST_TEXT) {
            lv_obj_t *n = dash_label(row, 18, C_DIM, "muuda veebilehel");
            lv_obj_align(n, LV_ALIGN_RIGHT_MID, -14, 0);
            lv_obj_align(p.val[i], LV_ALIGN_RIGHT_MID, -170, 0);
            continue;
        }
        lv_obj_t *mi = button(row, "–", 70, 46, on_step, (void *)(intptr_t)(i << 1));
        lv_obj_align(mi, LV_ALIGN_RIGHT_MID, -356, 0);
        lv_obj_t *pl = button(row, "+", 70, 46, on_step, (void *)(intptr_t)(i << 1 | 1));
        lv_obj_align(pl, LV_ALIGN_RIGHT_MID, -8, 0);
    }
    /* jaotiste kiirnupud (pealkirja esimene sõna) */
    const int gap = 6, bw = (W - 24 - (p.n_heads - 1) * gap) / (p.n_heads ? p.n_heads : 1);
    for (int k = 0; k < p.n_heads; k++) {
        char t[24];
        snprintf(t, sizeof(t), "%s", lv_label_get_text(p.heads[k]));
        char *sp = strchr(t, ' ');
        if (sp) *sp = 0;
        lv_obj_t *b = button(v, t, bw, 42, on_jump, (void *)(intptr_t)k);
        lv_obj_set_style_text_font(lv_obj_get_child(b, 0), dash_font(18), 0);
        lv_obj_set_pos(b, 12 + k * (bw + gap), 0);
        p.jump[k] = b;
    }
    p.note = dash_label(v, 18, C_YELLOW, "");
    lv_obj_align(p.note, LV_ALIGN_BOTTOM_MID, 0, -14);
}

static void create_trip(lv_obj_t *v)
{
    static const char *L[] = { "LÄBISÕIT", "MOOTORITUNNID", "REIS", "REISI KÜTUS", "KESKMINE KULU",
                               "REISI SÕIDUAEG", "HETKE KULU", "PAAGIS (ECU)" };
    for (int i = 0; i < 8; i++) {
        int x = 24 + (i % 2) * 500, y = 10 + (i / 2) * 104;
        lv_obj_t *b = dash_box(v, x, y, 476, 92, C_PANEL, 10);
        lv_obj_set_style_border_color(b, C_LINE, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_t *l = dash_label(b, 18, C_DIM, L[i]);
        lv_obj_set_pos(l, 16, 8);
        p.trip[i] = dash_label(b, 46, C_TEXT, "--");
        lv_obj_align(p.trip[i], LV_ALIGN_BOTTOM_RIGHT, -16, -6);
    }
    lv_obj_t *b = button(v, "NULLI REIS", 260, 60, on_trip_reset, NULL);
    lv_obj_set_pos(b, (W - 260) / 2, 440);
}

static void create_log(lv_obj_t *v)
{
    lv_obj_t *list = lv_obj_create(v);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, W - 260, H - 70 - 20);
    lv_obj_set_pos(list, 16, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_style_bg_color(list, C_ACCENT, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_50, LV_PART_SCROLLBAR);
    bubble(list);
    p.log_l = dash_label(list, 22, C_TEXT, "");
    lv_obj_set_width(p.log_l, W - 290);
    p.log_n = -1;
    lv_obj_t *b = button(v, "KUSTUTA", 200, 60, on_log_clear, NULL);
    lv_obj_set_pos(b, W - 220, 10);
}

static void create_info(lv_obj_t *v)
{
    p.info_l = dash_label(v, 24, C_TEXT, "");
    lv_obj_set_pos(p.info_l, 24, 10);
    lv_obj_set_width(p.info_l, W - 290);
    lv_obj_t *b = button(v, "DEMO", 200, 60, on_demo, NULL);
    lv_obj_set_pos(b, W - 220, 10);
    b = button(v, "VAIKESEADED", 200, 60, on_defaults, NULL);
    lv_obj_set_pos(b, W - 220, 84);
    s_def_lbl = lv_obj_get_child(b, 0);
    lv_obj_set_style_text_color(s_def_lbl, C_YELLOW, 0);
    lv_obj_t *n = dash_label(v, 16, C_DIM, "taastab dash_config.h\nseaded (2 vajutust)");
    lv_obj_set_pos(n, W - 220, 150);
}

static void on_sd_test(lv_event_t *e)
{
    (void)e;
    logger_speed_test_start();
    p.dg_next = 0;
}

static lv_obj_t *dlabel(lv_obj_t *v, int x, int y, int px, lv_color_t c)
{
    lv_obj_t *l = dash_label(v, px, c, "");
    lv_obj_set_pos(l, x, y);
    return l;
}

static void create_diag(lv_obj_t *v)
{
    /* vasakul CAN */
    lv_obj_t *h = dash_label(v, 22, C_ACCENT, "CAN");
    lv_obj_set_pos(h, 16, 4);
    p.dg_can = dlabel(v, 70, 6, 20, C_TEXT);
    p.dg_cs = dlabel(v, 16, 34, 17, C_DIM);
    static const char *HD[4] = { "ID", "Hz", "ID", "Hz" };
    static const int CX[4] = { 16, 96, 300, 380 };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *t = dash_label(v, 16, C_DIM, HD[i]);
        lv_obj_set_pos(t, CX[i], 62);
        p.dg_col[i] = dlabel(v, CX[i], 84, 18, i % 2 ? C_TEXT : C_DIM);
        lv_obj_set_style_text_line_space(p.dg_col[i], -2, 0);
    }
    lv_obj_t *n = dash_label(v, 15, C_DIM, "– = MaxxECU kaader puudub");
    lv_obj_set_pos(n, 16, 500);

    /* paremal GPS, SD, süsteem */
    const int x = 560;
    h = dash_label(v, 22, C_ACCENT, "GPS");
    lv_obj_set_pos(h, x, 4);
    p.dg_gps = dlabel(v, x + 54, 6, 20, C_TEXT);
    lv_obj_set_width(p.dg_gps, W - x - 54 - 16);
    lv_label_set_long_mode(p.dg_gps, LV_LABEL_LONG_MODE_WRAP);
    p.dg_gpsl = dlabel(v, x, 62, 15, C_DIM);
    lv_obj_set_width(p.dg_gpsl, W - x - 16);
    lv_label_set_long_mode(p.dg_gpsl, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_height(p.dg_gpsl, 5 * 20);
    h = dash_label(v, 22, C_ACCENT, "SD-KAART");
    lv_obj_set_pos(h, x, 176);
    p.dg_sd = dlabel(v, x, 206, 20, C_TEXT);
    lv_obj_set_width(p.dg_sd, W - x - 16);
    lv_label_set_long_mode(p.dg_sd, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_t *b = button(v, "KIIRUSE TEST", 200, 48, on_sd_test, NULL);
    lv_obj_set_pos(b, x, 264);
    h = dash_label(v, 22, C_ACCENT, "SÜSTEEM");
    lv_obj_set_pos(h, x, 332);
    p.dg_sys = dlabel(v, x, 362, 18, C_TEXT);
    lv_obj_set_width(p.dg_sys, W - x - 16);
    lv_label_set_long_mode(p.dg_sys, LV_LABEL_LONG_MODE_WRAP);
}

static void update_diag(const dash_state_t *s)
{
    if (s->now_us < p.dg_next) return;
    p.dg_next = s->now_us + 500000;
    char b[600];

    /* CAN: kõik nähtud ID-d + puuduvad MaxxECU ID-d */
    static diag_id_t ids[DIAG_MAX_IDS];
    int n = diag_can_ids(ids, DIAG_MAX_IDS);
    uint16_t list[DIAG_MAX_IDS + 24];
    float hz[DIAG_MAX_IDS + 24];
    int m = 0, have = 0, want = 0;
    for (const uint16_t *e = maxxecu_frame_ids; *e; e++) {
        want++;
        float h = -1;
        for (int i = 0; i < n; i++) if (ids[i].id == *e && s->now_us - ids[i].last_us < 2000000) h = ids[i].hz;
        if (h >= 0) have++;
        list[m] = *e;
        hz[m++] = h;
    }
    for (int i = 0; i < n && m < DIAG_MAX_IDS + 24; i++) {
        bool maxx = false;
        for (const uint16_t *e = maxxecu_frame_ids; *e; e++) maxx |= *e == ids[i].id;
        if (maxx) continue;
        list[m] = ids[i].id;
        hz[m++] = s->now_us - ids[i].last_us < 2000000 ? ids[i].hz : 0;
    }
    /* sorteeri ID järgi */
    for (int i = 1; i < m; i++)
        for (int k = i; k > 0 && list[k - 1] > list[k]; k--) {
            uint16_t t = list[k]; list[k] = list[k - 1]; list[k - 1] = t;
            float f = hz[k]; hz[k] = hz[k - 1]; hz[k - 1] = f;
        }
    const int per = 18;
    for (int c = 0; c < 2; c++) {
        char a[400] = "", z[400] = "";
        for (int r = 0; r < per; r++) {
            int i = c * per + r;
            if (i >= m) break;
            size_t la = strlen(a), lz = strlen(z);
            snprintf(a + la, sizeof(a) - la, "%s0x%03X", r ? "\n" : "", list[i]);
            if (hz[i] < 0) snprintf(z + lz, sizeof(z) - lz, "%s–", r ? "\n" : "");
            else snprintf(z + lz, sizeof(z) - lz, "%s%.0f", r ? "\n" : "", hz[i]);
        }
        dash_set_text_if(p.dg_col[c * 2], p.t_dg_col[c * 2], sizeof(p.t_dg_col[0]), a);
        dash_set_text_if(p.dg_col[c * 2 + 1], p.t_dg_col[c * 2 + 1], sizeof(p.t_dg_col[0]), z);
    }
    snprintf(b, sizeof(b), "MaxxECU %d/%d kaadrit  ·  %.0f kaadrit/s  ·  bus-off %u  ·  kadunud %u", have, want,
             diag_can_total_hz(), (unsigned)diag_can_busoffs(), (unsigned)diag_can_overflows());
    dash_set_text_if(p.dg_can, p.t_dg_can, sizeof(p.t_dg_can), b);
    lv_obj_set_style_text_color(p.dg_can, have == want ? C_GREEN : have ? C_YELLOW : C_RED, 0);
    diag_can_state_text(b, sizeof(b));
    if (!b[0]) snprintf(b, sizeof(b), "kokku %u kaadrit", (unsigned)diag_can_total());
    dash_set_text_if(p.dg_cs, p.t_dg_cs, sizeof(p.t_dg_cs), b);

    /* GPS */
    const gps_data_t *g = &s->gps;
    if (!g->connected) snprintf(b, sizeof(b), "andmeid ei tule (kontrolli TX juhet ja pinne)");
    else {
        int l = snprintf(b, sizeof(b), "%s  ·  %d sat  ·  HDOP %.1f  ·  %.0f Hz", g->fix ? "asukoht olemas" : "otsib",
                         g->sats, g->hdop, g->rate_hz);
        if (g->baud) l += snprintf(b + l, sizeof(b) - l, "  ·  %d baud", g->baud);
        snprintf(b + l, sizeof(b) - l, "\nlauseid %u, vigaseid %u", (unsigned)diag_gps_good(), (unsigned)diag_gps_bad());
    }
    dash_set_text_if(p.dg_gps, p.t_dg_gps, sizeof(p.t_dg_gps), b);
    static char lines[DIAG_GPS_LINES][100];
    int nl = diag_gps_lines(lines, DIAG_GPS_LINES);
    b[0] = 0;
    for (int i = 0; i < nl; i++) {
        size_t l = strlen(b);
        snprintf(b + l, sizeof(b) - l, "%s%s", i ? "\n" : "", lines[i]);
    }
    if (!nl) snprintf(b, sizeof(b), "(NMEA lauseid pole tulnud)");
    dash_set_text_if(p.dg_gpsl, p.t_dg_gpsl, sizeof(p.t_dg_gpsl), b);

    /* SD */
    uint64_t tot, fr;
    logger_status_t ls;
    logger_status(&ls);
    if (!logger_space(&tot, &fr)) snprintf(b, sizeof(b), "kaarti pole (või ei saa lugeda)");
    else {
        int l = snprintf(b, sizeof(b), "%.1f GB, vaba %.1f GB%s", tot / 1e9, fr / 1e9, ls.error ? "  ·  KIRJUTUSVIGA" : "");
        float sp = logger_speed_result();
        if (sp > 0) snprintf(b + l, sizeof(b) - l, "  ·  kirjutus %.0f kB/s%s", sp, sp < 200 ? " (aeglane!)" : "");
        else if (sp == -2) snprintf(b + l, sizeof(b) - l, "  ·  testin…");
        else if (sp == -1) snprintf(b + l, sizeof(b) - l, "  ·  test EBAÕNNESTUS");
    }
    dash_set_text_if(p.dg_sd, p.t_dg_sd, sizeof(p.t_dg_sd), b);

    diag_system(b, sizeof(b));
    dash_set_text_if(p.dg_sys, p.t_dg_sys, sizeof(p.t_dg_sys), b);
}

static void create(lv_obj_t *scr)
{
    for (int i = 0; i < N_TABS; i++) {
        lv_obj_t *b = button(scr, TAB_TXT[i], 158, 50, on_tab, (void *)(intptr_t)i);
        lv_obj_set_pos(b, 12 + i * 168, 10);
        p.tabs[i] = b;
        p.view[i] = view_box(scr);
    }
    ecu_panel_create(p.view[TAB_ECU]);
    create_settings(p.view[TAB_SET]);
    create_trip(p.view[TAB_TRIP]);
    create_log(p.view[TAB_LOG]);
    create_info(p.view[TAB_INFO]);
    create_diag(p.view[TAB_DIAG]);
    show_tab(TAB_ECU);
}

static void fmt_hms(uint32_t s, char *buf, int cap)
{
    snprintf(buf, cap, "%u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60));
}

static void update_trip(const dash_state_t *s)
{
    const dash_stats_t *st = stats_get();
    char b[32];
    snprintf(b, sizeof(b), "%.0f km", st->odo_m / 1000);
    dash_set_text_if(p.trip[0], p.t_trip[0], 32, b);
    snprintf(b, sizeof(b), "%.1f h", st->engine_s / 3600.0);
    dash_set_text_if(p.trip[1], p.t_trip[1], 32, b);
    snprintf(b, sizeof(b), "%.1f km", st->trip_m / 1000);
    dash_set_text_if(p.trip[2], p.t_trip[2], 32, b);
    snprintf(b, sizeof(b), "%.1f L", st->trip_fuel_l);
    dash_set_text_if(p.trip[3], p.t_trip[3], 32, b);
    float avg = stats_trip_avg();
    if (avg >= 0) snprintf(b, sizeof(b), "%.1f L/100km", avg);
    else snprintf(b, sizeof(b), "--");
    dash_set_text_if(p.trip[4], p.t_trip[4], 32, b);
    fmt_hms(st->trip_s, b, sizeof(b));
    dash_set_text_if(p.trip[5], p.t_trip[5], 32, b);
    bool per_km;
    float now = stats_fuel_now(s, &per_km);
    if (now < 0) snprintf(b, sizeof(b), "--");
    else snprintf(b, sizeof(b), "%.1f %s", now, per_km ? "L/100km" : "L/h");
    dash_set_text_if(p.trip[6], p.t_trip[6], 32, b);
    if (s->d.valid[SIG_FUEL_TANK]) snprintf(b, sizeof(b), "%.1f L", s->d.v[SIG_FUEL_TANK]);
    else snprintf(b, sizeof(b), "--");
    dash_set_text_if(p.trip[7], p.t_trip[7], 32, b);
}

static void update_log(void)
{
    int n = alarms_log_count();
    int stamp = n * 1000 + alarms_new_count();
    if (stamp == p.log_n) return;
    p.log_n = stamp;
    static char buf[2048];
    buf[0] = 0;
    for (int i = 0; i < n; i++) {
        const alarm_event_t *e = alarms_log_get(i);
        char when[40], val[48];
        if (e->year) snprintf(when, sizeof(when), "%02d.%02d %02d:%02d UTC", e->day, e->mon, e->hour, e->min);
        else snprintf(when, sizeof(when), "%.1f h mootorit", e->engine_s / 3600.0);
        val[0] = 0;
        if (e->kind == 0 && !isnan(e->limit)) snprintf(val, sizeof(val), "  %.1f (piir %.1f)", e->value, e->limit);
        else if (e->kind == 1 && e->value) snprintf(val, sizeof(val), "  (%.0f)", e->value);
        size_t l = strlen(buf);
        if (l > sizeof(buf) - 120) break;
        snprintf(buf + l, sizeof(buf) - l, "%s%s   %s%s   %d rpm", i ? "\n" : "", when, alarms_event_label(e), val,
                 e->rpm);
    }
    if (!n) snprintf(buf, sizeof(buf), "Hoiatusi pole olnud.");
    dash_set_text_if(p.log_l, p.t_log, sizeof(p.t_log), buf);
}

static void update_info(const dash_state_t *s)
{
    char b[600], wifi[120];
    logger_status_t ls;
    logger_status(&ls);
    wifi_web_status(wifi, sizeof(wifi));
    const gps_data_t *g = &s->gps;
    char gps[120];
    if (!g->connected) snprintf(gps, sizeof(gps), "ei ole ühendatud");
    else snprintf(gps, sizeof(gps), "%s, %d satelliiti, %.0f Hz%s%s", g->fix ? "asukoht olemas" : "otsib",
                  g->sats, g->rate_hz, g->baud ? ", " : "", g->baud ? "" : "");
    if (g->connected && g->baud) {
        size_t l = strlen(gps);
        snprintf(gps + l, sizeof(gps) - l, "%d baud", g->baud);
    }
    char sd[120];
    if (!ls.card) snprintf(sd, sizeof(sd), "kaarti pole");
    else if (ls.active) snprintf(sd, sizeof(sd), "logib: %s (%u rida%s)", ls.file, (unsigned)ls.rows, ls.dropped ? ", osa kadus" : "");
    else snprintf(sd, sizeof(sd), "valmis%s%s", ls.file[0] ? ", viimane " : "", ls.file);
    snprintf(b, sizeof(b),
             "SimplDash v" SIMPLDASH_VERSION "\n\n"
             "ECU:  %s, %u kaadrit\n"
             "GPS:  %s\n"
             "SD-kaart:  %s%s\n"
             "Wi-Fi:  %s\n"
             "ECU nupud:  %s",
             s->stale ? "andmed puuduvad" : "ühendatud", (unsigned)s->d.frame_count, gps, sd, ls.error ? " – KIRJUTUSVIGA" : "",
             wifi, g_set.can_tx_on ? "saadab" : "väljas");
    dash_set_text_if(p.info_l, p.t_info, sizeof(p.t_info), b);
}

static void update(const dash_state_t *s)
{
    now_us_cached = s->now_us;
    if ((s_def_ask_us && s->now_us - s_def_ask_us > 3000000) || (s_def_msg_until && s->now_us > s_def_msg_until)) {
        s_def_ask_us = s_def_msg_until = 0;
        lv_label_set_text(s_def_lbl, "VAIKESEADED");
    }
    if (p.tab == TAB_ECU) {
        ecu_panel_update(s);
    } else if (p.tab == TAB_SET) {
        char b[40];
        for (int i = 0; i < p.n_items; i++) {
            if (!p.val[i]) continue;
            settings_item_text(&p.items[i], b, sizeof(b));
            if (!strcmp(p.items[i].key, "wifi_pass")) snprintf(b, sizeof(b), "%s", b[0] ? "********" : "(puudub)");
            dash_set_text_if(p.val[i], p.t_val[i], sizeof(p.t_val[i]), b);
        }
        static char t_note[96];
        dash_set_text_if(p.note, t_note, sizeof(t_note),
                         p.restart_note ? "Osa muudatusi rakendub pärast taaskäivitust (süüde välja ja sisse)." : "");
    } else if (p.tab == TAB_TRIP) {
        update_trip(s);
    } else if (p.tab == TAB_LOG) {
        update_log();
    } else if (p.tab == TAB_INFO) {
        update_info(s);
    } else {
        update_diag(s);
    }
}

const dash_page_t page_settings = { "SEADED", 346, create, update, NULL, NULL, -1, 0, -1, 0, -1, 0 };
