/*
 * Vaade RADA (ainult GPS-iga): raja kaart parima ringi järgi, auto asukoht, start/finiš ja
 * sektorite piirid; paremal jooksev ring, vahe parimaga ja sektorite tabel.
 * MÄRGI START = start/finiš joon praegusesse kohta (uus rada); kaart joonistub esimese ringi ajal
 * GPS-i järgi ja sektorid pannakse automaatselt kolmandikesse. SEKTORID = puuduta kaardil
 * S1 ja S2 lõppu (käsitsi); AUTO = tagasi automaatseks.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dash_common.h"
#include "lap_core.h"

#define MAP_X 12
#define MAP_Y 12
#define MAP_W 600
#define MAP_H 576
#define MAP_PTS 400          /* kaardi joone punkte (harvendatud) */
#define MAP_PAD 20           /* raja ja kaardi serva vahe */
#define MAP_TOP 46           /* ülal ruum sektorite tekstile */

static struct {
    lv_obj_t *map, *line, *dot, *start, *gate[LAP_N_SECT - 1], *msg;
    lv_obj_t *b_start, *b_sect, *b_auto, *edit_lbl;
    int       edit;            /* -1 = ei muuda, 0/1 = ootab S1/S2 lõpu puudutust */
    int8_t    st_can, st_auto, st_edit;
    char      t_edit[80];
    lv_point_precise_t pts[MAP_PTS];
    int      n_pts, drawn_ref;     /* mitme punktiga võrdlusringi järgi on joonistatud */
    const lap_pt_t *drawn_buf;
    float    minx, miny, k;        /* meetrid → pikslid */
    lv_obj_t *time, *delta, *best, *sect[LAP_N_SECT][3];
    char     t_time[16], t_delta[16], t_best[16], t_msg[64], t_sect[LAP_N_SECT][3][12];
    int8_t   dcol;
} p;

static void to_px(float x, float y, int32_t *px, int32_t *py)
{
    *px = (int32_t)((x - p.minx) * p.k) + MAP_PAD;
    *py = MAP_H - MAP_PAD - (int32_t)((y - p.miny) * p.k);
}

static void place(lv_obj_t *o, float x, float y, int r)
{
    int32_t px, py;
    to_px(x, y, &px, &py);
    lv_obj_set_pos(o, px - r, py - r);
}

static void redraw(const lap_info_t *L)
{
    const lap_pt_t *t = L->n_ref > 1 ? L->ref : L->cur;
    int n = L->n_ref > 1 ? L->n_ref : L->n_cur;
    if (n < 2) {
        lv_obj_set_hidden(p.line, true);
        return;
    }
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    for (int i = 0; i < n; i++) {
        minx = fminf(minx, t[i].x); maxx = fmaxf(maxx, t[i].x);
        miny = fminf(miny, t[i].y); maxy = fmaxf(maxy, t[i].y);
    }
    const float aw = MAP_W - 2 * MAP_PAD, ah = MAP_H - MAP_TOP - MAP_PAD;
    float sx = aw / fmaxf(maxx - minx, 1), sy = ah / fmaxf(maxy - miny, 1);
    p.k = fminf(sx, sy);
    /* keskele */
    p.minx = minx - (aw / p.k - (maxx - minx)) / 2;
    p.miny = miny - (ah / p.k - (maxy - miny)) / 2;
    int step = n / MAP_PTS + 1, m = 0;
    for (int i = 0; i < n && m < MAP_PTS - 1; i += step) {
        int32_t px, py;
        to_px(t[i].x, t[i].y, &px, &py);
        p.pts[m++] = (lv_point_precise_t){ px, py };
    }
    if (L->n_ref > 1 && m < MAP_PTS) p.pts[m++] = p.pts[0];   /* ring kinni */
    lv_line_set_points(p.line, p.pts, (uint32_t)m);
    lv_obj_set_hidden(p.line, false);
    place(p.start, (float)L->line.cx, (float)L->line.cy, 9);
    lv_obj_set_hidden(p.start, false);
    for (int k = 0; k < LAP_N_SECT - 1; k++) {
        lv_obj_set_hidden(p.gate[k], !L->gate[k].set);
        if (L->gate[k].set) place(p.gate[k], (float)L->gate[k].cx, (float)L->gate[k].cy, 12);
    }
    p.drawn_ref = L->n_ref;
    p.drawn_buf = t;
}

static void map_to_m(int32_t px, int32_t py, float *x, float *y)
{
    *x = (px - MAP_PAD) / p.k + p.minx;
    *y = (MAP_H - MAP_PAD - py) / p.k + p.miny;
}

static void set_edit(int e)
{
    p.edit = e;
    lv_obj_set_clickable(p.map, e >= 0);
    lv_obj_set_style_border_color(p.map, e >= 0 ? C_YELLOW : C_LINE, 0);
    lv_obj_set_style_border_width(p.map, e >= 0 ? 3 : 1, 0);
    lv_label_set_text(lv_obj_get_child(p.b_sect, 0), e >= 0 ? "LOOBU" : "SEKTORID");
}

static void on_start(lv_event_t *e)
{
    (void)e;
    set_edit(-1);
    lap_mark_line();
    p.drawn_ref = -1;
}

static void on_sect(lv_event_t *e)
{
    (void)e;
    const lap_info_t *L = lap_info();
    if (p.edit >= 0) set_edit(-1);
    else if (L->n_ref > 1) set_edit(0);
}

static void on_auto(lv_event_t *e)
{
    (void)e;
    if (lap_info()->n_ref <= 1) return;
    set_edit(-1);
    lap_auto_gates();
    p.drawn_ref = -1;
}

/* kaardi puudutus muutmise ajal: lähim raja punkt saab sektori piiriks */
static void on_map(lv_event_t *e)
{
    (void)e;
    if (p.edit < 0 || p.k <= 0) return;
    lv_point_t pt;
    lv_indev_get_point(lv_indev_active(), &pt);
    lv_area_t a;
    lv_obj_get_coords(p.map, &a);
    float x, y;
    map_to_m(pt.x - a.x1, pt.y - a.y1, &x, &y);
    if (lap_set_gate(p.edit, x, y)) {
        p.drawn_ref = -1;   /* joonista markerid uuesti */
        set_edit(p.edit + 1 < LAP_N_SECT - 1 ? p.edit + 1 : -1);
    }
}

static lv_obj_t *small(lv_obj_t *scr, int x, int y, int px, lv_color_t c, const char *t)
{
    lv_obj_t *l = dash_label(scr, px, c, t);
    lv_obj_set_pos(l, x, y);
    return l;
}

static void create(lv_obj_t *scr)
{
    p.map = dash_box(scr, MAP_X, MAP_Y, MAP_W, MAP_H, C_PANEL, 12);
    lv_obj_set_style_border_color(p.map, C_LINE, 0);
    lv_obj_set_style_border_width(p.map, 1, 0);
    p.line = lv_line_create(p.map);
    lv_obj_set_style_line_width(p.line, 6, 0);
    lv_obj_set_style_line_color(p.line, lv_color_hex(0x55606E), 0);
    lv_obj_set_style_line_rounded(p.line, true, 0);
    lv_obj_set_clickable(p.line, false);
    lv_obj_set_hidden(p.line, true);
    p.start = dash_box(p.map, 0, 0, 18, 18, C_TEXT, 3);
    lv_obj_set_style_border_color(p.start, C_BG, 0);
    lv_obj_set_style_border_width(p.start, 3, 0);
    lv_obj_set_hidden(p.start, true);
    for (int k = 0; k < LAP_N_SECT - 1; k++) {
        p.gate[k] = dash_box(p.map, 0, 0, 24, 24, C_YELLOW, LV_RADIUS_CIRCLE);
        char t[4];
        snprintf(t, sizeof(t), "%d", k + 1);
        lv_obj_t *n = dash_label(p.gate[k], 16, C_BG, t);
        lv_obj_center(n);
        lv_obj_set_hidden(p.gate[k], true);
    }
    lv_obj_add_event_cb(p.map, on_map, LV_EVENT_CLICKED, NULL);
    p.edit_lbl = dash_label(p.map, 22, C_YELLOW, "");
    lv_obj_set_width(p.edit_lbl, MAP_W - 30);
    lv_obj_set_style_text_align(p.edit_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(p.edit_lbl, LV_ALIGN_TOP_MID, 0, 10);
    p.dot = dash_box(p.map, 0, 0, 22, 22, C_ACCENT, LV_RADIUS_CIRCLE);
    lv_obj_set_clickable(p.dot, false);
    lv_obj_set_style_border_color(p.dot, C_TEXT, 0);
    lv_obj_set_style_border_width(p.dot, 3, 0);
    lv_obj_set_hidden(p.dot, true);
    p.msg = dash_label(p.map, 24, C_DIM, "");
    lv_obj_set_width(p.msg, MAP_W - 40);
    lv_label_set_long_mode(p.msg, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(p.msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(p.msg, LV_ALIGN_CENTER, 0, 0);

    const int x = MAP_X + MAP_W + 24;
    small(scr, x, 14, 20, C_DIM, "RING");
    p.time = small(scr, x, 34, 76, C_TEXT, "0:00.00");
    small(scr, x, 130, 20, C_DIM, "VAHE PARIMAGA");
    p.delta = small(scr, x, 150, 64, C_DIM, "--");
    p.dcol = -1;
    small(scr, x, 236, 20, C_DIM, "PARIM RING");
    p.best = small(scr, x, 256, 44, C_YELLOW, "--");

    /* sektorite tabel */
    const int ty = 330;
    small(scr, x, ty, 18, C_DIM, "SEKTOR");
    small(scr, x + 110, ty, 18, C_DIM, "PRAEGU");
    small(scr, x + 220, ty, 18, C_DIM, "PARIM");
    for (int k = 0; k < LAP_N_SECT; k++) {
        char t[8];
        snprintf(t, sizeof(t), "S%d", k + 1);
        p.sect[k][0] = small(scr, x, ty + 30 + k * 46, 34, C_TEXT, t);
        p.sect[k][1] = small(scr, x + 110, ty + 30 + k * 46, 34, C_TEXT, "--");
        p.sect[k][2] = small(scr, x + 220, ty + 30 + k * 46, 34, C_YELLOW, "--");
    }

    /* nupud */
    const int bw = (W - 12 - x - 2 * 8) / 3, by = 512, bh = 76;
    p.b_start = dash_button(scr, "MÄRGI START", x, by, bw, bh, on_start, NULL);
    p.b_sect = dash_button(scr, "SEKTORID", x + bw + 8, by, bw, bh, on_sect, NULL);
    p.b_auto = dash_button(scr, "AUTO", x + 2 * (bw + 8), by, bw, bh, on_auto, NULL);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = i == 0 ? p.b_start : i == 1 ? p.b_sect : p.b_auto;
        lv_obj_set_style_text_font(lv_obj_get_child(b, 0), dash_font(20), 0);
        lv_obj_set_event_bubble(b, true);
    }
    p.edit = -1;
    p.st_can = p.st_auto = p.st_edit = -1;
}

static void update(const dash_state_t *s)
{
    const lap_info_t *L = lap_info();
    char buf[64];

    if (L->n_ref != p.drawn_ref || (L->n_ref <= 1 && L->n_cur / 20 != p.n_pts)) {
        p.n_pts = L->n_cur / 20;   /* esimesel ringil joonista jooksvalt (iga 20 punkti järel) */
        redraw(L);
    }
    bool have_map = !lv_obj_is_hidden(p.line);
    if (!s->gps.fix) snprintf(buf, sizeof(buf), "OOTAN GPS SIGNAALI");
    else if (!L->line_set) snprintf(buf, sizeof(buf), "Sõida start/finiš joonele ja vajuta MÄRGI START");
    else if (!have_map) snprintf(buf, sizeof(buf), "Sõida üks ring – raja kaart joonistub GPS-i järgi");
    else buf[0] = 0;
    dash_set_text_if(p.msg, p.t_msg, sizeof(p.t_msg), buf);

    /* sektorite muutmine */
    if (p.edit >= 0) snprintf(buf, sizeof(buf), "Puuduta kaardil kohta, kus lõpeb S%d", p.edit + 1);
    else if (L->n_ref > 1) snprintf(buf, sizeof(buf), "%s", L->gates_manual ? "sektorid: käsitsi" : "sektorid: automaatsed");
    else buf[0] = 0;
    dash_set_text_if(p.edit_lbl, p.t_edit, sizeof(p.t_edit), buf);
    if (p.st_edit != (p.edit >= 0)) {
        p.st_edit = p.edit >= 0;
        lv_obj_set_style_text_color(p.edit_lbl, p.st_edit ? C_YELLOW : C_DIM, 0);
    }
    /* SEKTORID / AUTO ainult siis, kui raja kuju on olemas */
    bool can = L->n_ref > 1;
    if (p.st_can != can) {
        p.st_can = can;
        lv_obj_set_style_opa(p.b_sect, can ? LV_OPA_COVER : LV_OPA_40, 0);
    }
    if (p.st_auto != (can && L->gates_manual)) {
        p.st_auto = can && L->gates_manual;
        lv_obj_set_style_opa(p.b_auto, p.st_auto ? LV_OPA_COVER : LV_OPA_40, 0);
    }
    if (!can && p.edit >= 0) set_edit(-1);
    lv_obj_set_hidden(p.dot, !(have_map && L->pos_ok));
    if (have_map && L->pos_ok) place(p.dot, L->pos_x, L->pos_y, 11);

    if (L->running) dash_fmt_time((s->now_us - L->lap_start) / 1e6f, true, buf, 16);
    else snprintf(buf, 16, "0:00.00");
    dash_set_text_if(p.time, p.t_time, sizeof(p.t_time), buf);
    int8_t dc = 0;
    if (L->running && L->delta_ok) {
        snprintf(buf, 16, "%+.2f", L->delta);
        dc = L->delta < 0 ? 1 : 2;
    } else {
        snprintf(buf, 16, "--");
    }
    dash_set_text_if(p.delta, p.t_delta, sizeof(p.t_delta), buf);
    if (dc != p.dcol) {
        lv_obj_set_style_text_color(p.delta, dc == 1 ? C_GREEN : dc == 2 ? C_RED : C_DIM, 0);
        p.dcol = dc;
    }
    if (L->best > 0) dash_fmt_time(L->best, true, buf, 16);
    else snprintf(buf, 16, "--");
    dash_set_text_if(p.best, p.t_best, sizeof(p.t_best), buf);

    for (int k = 0; k < LAP_N_SECT; k++) {
        /* praegu: käimasoleva ringi valmis sektor, muidu eelmise ringi oma */
        float v = L->sec_cur[k] > 0 ? L->sec_cur[k] : (k >= L->sect ? L->sec_last[k] : 0);
        if (v > 0) snprintf(buf, 12, "%.2f", v);
        else snprintf(buf, 12, "--");
        dash_set_text_if(p.sect[k][1], p.t_sect[k][1], 12, buf);
        if (L->sec_best[k] > 0) snprintf(buf, 12, "%.2f", L->sec_best[k]);
        else snprintf(buf, 12, "--");
        dash_set_text_if(p.sect[k][2], p.t_sect[k][2], 12, buf);
        lv_obj_set_style_text_color(p.sect[k][0], L->running && L->sect == k ? C_ACCENT : C_TEXT, 0);
    }
}

const dash_page_t page_track = { "RADA", 346, create, update, NULL, dash_need_gps, 938, 4, -1, 0, 856, 4 };
