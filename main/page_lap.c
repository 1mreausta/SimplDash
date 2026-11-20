/*
 * Vaade RING: ringiaja taimer GPS-i järgi (nähtav ainult GPS-iga).
 * "MÄRGI JOON" salvestab praeguse asukoha ja sõidusuuna start/finiš jooneks ning alustab ringi.
 * Ring lõpeb, kui auto ületab joone samas suunas (±DASH_LAP_GATE_M joone keskelt).
 * Ületamise hetk interpoleeritakse kahe GPS-mõõtmise vahel.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dash_common.h"
#include "gps_pages.h"

#include "lap_core.h"

static const dash_tile_t TL[] = DASH_LAP_LEFT;
static const dash_tile_t TR[] = DASH_LAP_RIGHT;
#define N_TL (sizeof(TL) / sizeof(TL[0]))
#define N_TR (sizeof(TR) / sizeof(TR[0]))

static struct {
    lv_obj_t *time, *info, *hint, *last_v, *best_v, *delta_v, *hist_l, *live_d, *sect_l;
    char     t_time[16], t_info[40], t_last[16], t_best[16], t_delta[16], t_hist[128], t_live[16], t_sect[64];
    int8_t   hint_shown, delta_col, live_col;
    dash_tile_obj_t tiles[N_TL + N_TR];
    bool     time_on;
} p;

static void background(const dash_state_t *s)
{
    lap_sample(s);
}

static void on_mark(lv_event_t *e)
{
    (void)e;
    lap_mark_line();
}

static void on_reset(lv_event_t *e)
{
    (void)e;
    lap_reset_times();
}

static void create(lv_obj_t *scr)
{
    lv_obj_t *title = dash_label(scr, 30, C_DIM, "RINGIAEG");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    /* külgedel 4 + 4 näidikut, keskel ringiaeg */
    const int tw = 200, th = 86, tg = 8, ty = 46;
    for (size_t i = 0; i < N_TL; i++) {
        dash_tile_create(&p.tiles[i], &TL[i], scr, 12, ty + i * (th + tg), tw, th, 44);
    }
    for (size_t i = 0; i < N_TR; i++) {
        dash_tile_create(&p.tiles[N_TL + i], &TR[i], scr, W - 12 - tw, ty + i * (th + tg), tw, th, 44);
    }
    const int cx = 12 + tw + 12, cw = W - 2 * cx;

    p.time = dash_label(scr, 140, C_DIM, "0:00.00");
    lv_obj_set_width(p.time, cw);
    lv_obj_set_style_text_align(p.time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.time, cx, 70);

    p.info = dash_label(scr, 30, C_DIM, "");
    lv_obj_set_width(p.info, cw);
    lv_obj_set_style_text_align(p.info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.info, cx, 250);

    /* vahe parima ringiga samas kohas rajal: roheline = kiirem, punane = aeglasem */
    p.live_d = dash_label(scr, 64, C_DIM, "");
    lv_obj_set_width(p.live_d, cw);
    lv_obj_set_style_text_align(p.live_d, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.live_d, cx, 282);
    p.live_col = -1;
    p.sect_l = dash_label(scr, 24, C_DIM, "");
    lv_obj_set_width(p.sect_l, cw);
    lv_obj_set_style_text_align(p.sect_l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.sect_l, cx, 372);

    p.hint = dash_label(scr, 24, C_YELLOW, "Sõida start/finiš joonele ja vajuta  MÄRGI JOON");
    lv_obj_set_width(p.hint, cw);
    lv_label_set_long_mode(p.hint, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(p.hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.hint, cx, 336);
    p.hint_shown = -1;

    lv_obj_t *b1 = dash_panel(scr, 12, 240, "VIIMANE RING");
    p.last_v = dash_label(b1, 52, C_TEXT, "--");
    lv_obj_align(p.last_v, LV_ALIGN_BOTTOM_RIGHT, -14, -34);
    p.delta_v = dash_label(b1, 24, C_DIM, "");
    lv_obj_align(p.delta_v, LV_ALIGN_BOTTOM_RIGHT, -14, -6);
    p.delta_col = -1;
    lv_obj_t *b2 = dash_panel(scr, 264, 240, "PARIM RING");
    p.best_v = dash_label(b2, 52, C_YELLOW, "--");
    lv_obj_align(p.best_v, LV_ALIGN_BOTTOM_RIGHT, -14, -34);
    lv_obj_t *b3 = dash_panel(scr, 516, 300, "RINGID");
    p.hist_l = dash_label(b3, 22, C_TEXT, "");
    lv_obj_set_pos(p.hist_l, 14, 30);

    dash_button(scr, "MÄRGI JOON", 828, 430, 184, 74, on_mark, NULL);
    dash_button(scr, "NULLI AJAD", 828, 512, 184, 74, on_reset, NULL);
}

static void update(const dash_state_t *s)
{
    const lap_info_t *L = lap_info();
    char buf[128];
    for (size_t i = 0; i < N_TL + N_TR; i++) dash_tile_update(&p.tiles[i], s);
    if (L->running) dash_fmt_time((s->now_us - L->lap_start) / 1e6f, true, buf, 16);
    else snprintf(buf, 16, "0:00.00");
    dash_set_text_if(p.time, p.t_time, sizeof(p.t_time), buf);
    if (L->running != p.time_on) {
        lv_obj_set_style_text_color(p.time, L->running ? C_TEXT : C_DIM, 0);
        p.time_on = L->running;
    }

    if (!s->gps.fix) snprintf(buf, 40, "OOTAN GPS SIGNAALI");
    else if (L->running) snprintf(buf, 40, "RING %d", L->laps);
    else if (L->line_set) snprintf(buf, 40, "sõida üle start/finiš joone");
    else buf[0] = 0;
    dash_set_text_if(p.info, p.t_info, sizeof(p.t_info), buf);

    int8_t hint = !L->line_set;
    if (hint != p.hint_shown) {
        lv_obj_set_hidden(p.hint, !hint);
        p.hint_shown = hint;
    }

    /* jooksev vahe parima ringiga */
    int8_t lc = 0;
    if (L->running && L->delta_ok) {
        snprintf(buf, 16, "%+.2f", L->delta);
        lc = L->delta < 0 ? 1 : 2;
    } else {
        buf[0] = 0;
    }
    dash_set_text_if(p.live_d, p.t_live, sizeof(p.t_live), buf);
    if (lc != p.live_col) {
        lv_obj_set_style_text_color(p.live_d, lc == 1 ? C_GREEN : lc == 2 ? C_RED : C_DIM, 0);
        p.live_col = lc;
    }
    /* sektorid: käimasolev ring (valmis sektorid), muidu eelmine */
    buf[0] = 0;
    if (L->gate[0].set) {
        for (int k = 0; k < LAP_N_SECT; k++) {
            float v = L->sec_cur[k] > 0 ? L->sec_cur[k] : 0;
            size_t l = strlen(buf);
            if (v > 0) snprintf(buf + l, sizeof(buf) - l, "%sS%d %.2f%s", k ? "   " : "", k + 1, v,
                                L->sec_best[k] > 0 && v <= L->sec_best[k] + 0.005f ? "*" : "");
            else snprintf(buf + l, sizeof(buf) - l, "%sS%d --", k ? "   " : "", k + 1);
        }
    }
    dash_set_text_if(p.sect_l, p.t_sect, sizeof(p.t_sect), buf);

    if (L->last > 0) dash_fmt_time(L->last, true, buf, 16);
    else snprintf(buf, 16, "--");
    dash_set_text_if(p.last_v, p.t_last, sizeof(p.t_last), buf);
    if (L->best > 0) dash_fmt_time(L->best, true, buf, 16);
    else snprintf(buf, 16, "--");
    dash_set_text_if(p.best_v, p.t_best, sizeof(p.t_best), buf);

    /* viimase ringi vahe eelmise parimaga: roheline = uus parim */
    int8_t col = 0;
    if (L->last > 0 && L->prev_best > 0) {
        float d = L->last - L->prev_best;
        snprintf(buf, 16, "%+.2f", d);
        col = d < 0 ? 1 : 2;
    } else {
        buf[0] = 0;
    }
    dash_set_text_if(p.delta_v, p.t_delta, sizeof(p.t_delta), buf);
    if (col != p.delta_col) {
        lv_obj_set_style_text_color(p.delta_v, col == 1 ? C_GREEN : col == 2 ? C_RED : C_DIM, 0);
        p.delta_col = col;
    }

    buf[0] = 0;
    for (int i = 0; i < L->n_hist; i++) {
        char t[16];
        dash_fmt_time(L->hist[i], true, t, sizeof(t));
        size_t l = strlen(buf);
        snprintf(buf + l, sizeof(buf) - l, "%s%d.  %s%s", i ? "\n" : "", L->done - i, t,
                 L->hist[i] == L->best ? "   parim" : "");
    }
    if (!L->n_hist) snprintf(buf, sizeof(buf), "–");
    dash_set_text_if(p.hist_l, p.t_hist, sizeof(p.t_hist), buf);
}

const dash_page_t page_lap = { "RING", 346, create, update, background, dash_need_gps, 938, 4, 16, 4, 856, 4 };
