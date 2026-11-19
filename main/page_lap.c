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

#define N_HIST 5
#define DEG2RAD 0.017453292519943295

static struct {
    /* joon ja ringid */
    bool     line_set, running;
    double   lat0, lon0, dx, dy;      /* joone keskpunkt, sõidusuuna ühikvektor (ida, põhi) */
    double   mlat, mlon;              /* meetrit kraadi kohta */
    uint32_t last_sample;
    double   px, py;                  /* eelmine asukoht meetrites */
    int64_t  pt, lap_start;
    bool     have_prev;
    float    last, best, prev_best;
    float    hist[N_HIST];
    int      n_hist, laps;
    float    moving_course;
    /* ekraan */
    lv_obj_t *time, *info, *hint, *last_v, *best_v, *delta_v, *hist_l;
    char     t_time[16], t_info[40], t_last[16], t_best[16], t_delta[16], t_hist[96];
    int8_t   hint_shown, delta_col;
    bool     time_on;
} p;

static void to_xy(double lat, double lon, double *x, double *y)
{
    *x = (lon - p.lon0) * p.mlon;
    *y = (lat - p.lat0) * p.mlat;
}

static void background(const dash_state_t *s)
{
    const gps_data_t *g = &s->gps;
    if (!g->fix) {
        p.have_prev = false;
        p.last_sample = g->samples;
        return;
    }
    if (g->samples == p.last_sample) return;
    p.last_sample = g->samples;
    if (g->speed_kmh > 5) p.moving_course = g->course;
    if (!p.line_set) return;

    double x, y;
    to_xy(g->lat, g->lon, &x, &y);
    int64_t t = g->sample_us;
    if (p.have_prev) {
        /* märgiga kaugus joonest sõidusuunas: eelmine < 0, praegune >= 0 → ületas */
        double sa = p.px * p.dx + p.py * p.dy;
        double sb = x * p.dx + y * p.dy;
        if (sa < 0 && sb >= 0) {
            double f = sa != sb ? -sa / (sb - sa) : 1.0;
            double cx = p.px + f * (x - p.px), cy = p.py + f * (y - p.py);
            double lateral = fabs(cx * p.dy - cy * p.dx);
            int64_t tc = p.pt + (int64_t)(f * (double)(t - p.pt));
            float lap = (tc - p.lap_start) / 1e6f;
            if (lateral <= DASH_LAP_GATE_M && (!p.running || lap >= DASH_LAP_MIN_S)) {
                if (p.running) {
                    p.last = lap;
                    p.prev_best = p.best;
                    if (p.best <= 0 || lap < p.best) p.best = lap;
                    memmove(&p.hist[1], &p.hist[0], sizeof(float) * (N_HIST - 1));
                    p.hist[0] = lap;
                    if (p.n_hist < N_HIST) p.n_hist++;
                }
                p.laps++;
                p.lap_start = tc;
                p.running = true;
            }
        }
    }
    p.px = x;
    p.py = y;
    p.pt = t;
    p.have_prev = true;
}

static void on_mark(lv_event_t *e)
{
    (void)e;
    gps_data_t g;
    gps_snapshot(&g);
    if (!g.fix) return;
    double c = (g.speed_kmh > 5 ? g.course : p.moving_course) * DEG2RAD;
    p.lat0 = g.lat;
    p.lon0 = g.lon;
    p.dx = sin(c);
    p.dy = cos(c);
    p.mlat = 110540.0;
    p.mlon = 111320.0 * cos(g.lat * DEG2RAD);
    p.line_set = true;
    p.running = true;                 /* ring algab märkimise hetkest */
    p.lap_start = g.sample_us;
    p.laps = 1;
    p.px = p.py = 0;
    p.pt = g.sample_us;
    p.have_prev = true;
    p.last_sample = g.samples;
}

static void on_reset(lv_event_t *e)
{
    (void)e;
    p.line_set = p.running = false;
    p.last = p.best = p.prev_best = 0;
    p.n_hist = p.laps = 0;
}

static lv_obj_t *panel(lv_obj_t *scr, int x, int w, const char *title)
{
    lv_obj_t *b = dash_box(scr, x, 430, w, 156, C_PANEL, 10);
    lv_obj_set_style_border_color(b, C_LINE, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_t *t = dash_label(b, 17, C_DIM, title);
    lv_obj_set_pos(t, 14, 8);
    return b;
}

static void create(lv_obj_t *scr)
{
    lv_obj_t *title = dash_label(scr, 30, C_DIM, "RINGIAEG");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    p.time = dash_label(scr, 170, C_DIM, "0:00.00");
    lv_obj_set_width(p.time, 900);
    lv_obj_set_style_text_align(p.time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.time, (W - 900) / 2, 76);

    p.info = dash_label(scr, 34, C_DIM, "");
    lv_obj_set_width(p.info, 600);
    lv_obj_set_style_text_align(p.info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.info, (W - 600) / 2, 290);

    p.hint = dash_label(scr, 24, C_YELLOW, "Sõida start/finiš joonele ja vajuta  MÄRGI JOON");
    lv_obj_set_width(p.hint, 900);
    lv_obj_set_style_text_align(p.hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.hint, (W - 900) / 2, 352);
    p.hint_shown = -1;

    lv_obj_t *b1 = panel(scr, 12, 240, "VIIMANE RING");
    p.last_v = dash_label(b1, 52, C_TEXT, "--");
    lv_obj_align(p.last_v, LV_ALIGN_BOTTOM_RIGHT, -14, -34);
    p.delta_v = dash_label(b1, 24, C_DIM, "");
    lv_obj_align(p.delta_v, LV_ALIGN_BOTTOM_RIGHT, -14, -6);
    p.delta_col = -1;
    lv_obj_t *b2 = panel(scr, 264, 240, "PARIM RING");
    p.best_v = dash_label(b2, 52, C_YELLOW, "--");
    lv_obj_align(p.best_v, LV_ALIGN_BOTTOM_RIGHT, -14, -34);
    lv_obj_t *b3 = panel(scr, 516, 300, "RINGID");
    p.hist_l = dash_label(b3, 22, C_TEXT, "");
    lv_obj_set_pos(p.hist_l, 14, 30);

    dash_button(scr, "MÄRGI JOON", 828, 430, 184, 74, on_mark);
    dash_button(scr, "NULLI", 828, 512, 184, 74, on_reset);
}

static void update(const dash_state_t *s)
{
    char buf[96];
    if (p.running) dash_fmt_time((s->now_us - p.lap_start) / 1e6f, true, buf, 16);
    else snprintf(buf, 16, "0:00.00");
    dash_set_text_if(p.time, p.t_time, sizeof(p.t_time), buf);
    if (p.running != p.time_on) {
        lv_obj_set_style_text_color(p.time, p.running ? C_TEXT : C_DIM, 0);
        p.time_on = p.running;
    }

    if (!s->gps.fix) snprintf(buf, 40, "OOTAN GPS SIGNAALI");
    else if (p.running) snprintf(buf, 40, "RING %d", p.laps);
    else buf[0] = 0;
    dash_set_text_if(p.info, p.t_info, sizeof(p.t_info), buf);

    int8_t hint = !p.line_set;
    if (hint != p.hint_shown) {
        lv_obj_set_hidden(p.hint, !hint);
        p.hint_shown = hint;
    }

    if (p.last > 0) dash_fmt_time(p.last, true, buf, 16);
    else snprintf(buf, 16, "--");
    dash_set_text_if(p.last_v, p.t_last, sizeof(p.t_last), buf);
    if (p.best > 0) dash_fmt_time(p.best, true, buf, 16);
    else snprintf(buf, 16, "--");
    dash_set_text_if(p.best_v, p.t_best, sizeof(p.t_best), buf);

    /* viimase ringi vahe eelmise parimaga: roheline = uus parim */
    int8_t col = 0;
    if (p.last > 0 && p.prev_best > 0) {
        float d = p.last - p.prev_best;
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
    for (int i = 0; i < p.n_hist; i++) {
        char t[16];
        dash_fmt_time(p.hist[i], true, t, sizeof(t));
        size_t l = strlen(buf);
        snprintf(buf + l, sizeof(buf) - l, "%s%d.  %s%s", i ? "\n" : "", p.laps - 1 - i, t,
                 p.hist[i] == p.best ? "   parim" : "");
    }
    if (!p.n_hist) snprintf(buf, sizeof(buf), "–");
    dash_set_text_if(p.hist_l, p.t_hist, sizeof(p.t_hist), buf);
}

const dash_page_t page_lap = { "RING", 470, create, update, background, dash_need_gps, 846, 6, 16, 4 };
