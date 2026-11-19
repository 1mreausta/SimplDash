/*
 * Vaade 0–100: kiirendusaja mõõtmine GPS-kiiruse järgi (nähtav ainult GPS-iga).
 * Peatu ≥ 1 s → VALMIS; sõida välja → mõõdab; 100 km/h → tulemus. Ajad interpoleeritakse
 * GPS-mõõtmiste vahel, seega täpsus on parem kui GPS-i uuendusintervall.
 */
#include <stdio.h>
#include <string.h>
#include "dash_common.h"
#include "gps_pages.h"

#define N_HIST 5

enum { ST_NOFIX, ST_WAIT, ST_READY, ST_RUN, ST_DONE };

static struct {
    /* mõõtmine */
    int      st;
    uint32_t last_sample;
    int64_t  t_prev, still_since, t_start, t_split;
    float    v_prev;
    float    result, split, best;
    float    hist[N_HIST];
    int      n_hist;
    /* ekraan */
    lv_obj_t *time, *status, *bar, *speed, *split_v, *best_v, *hist_l;
    char     t_time[16], t_status[40], t_speed[16], t_split_v[16], t_best_v[16], t_hist[96];
    int8_t   status_col;
} p;

/* hetk, mil kiirus ületas target-i (lineaarne interpolatsioon kahe mõõtmise vahel) */
static int64_t interp_t(int64_t t0, float v0, int64_t t1, float v1, float target)
{
    double f = v1 > v0 ? (target - v0) / (double)(v1 - v0) : 1.0;
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    return t0 + (int64_t)(f * (double)(t1 - t0));
}

static void background(const dash_state_t *s)
{
    const gps_data_t *g = &s->gps;
    if (!g->fix) {
        if (p.st != ST_DONE) p.st = ST_NOFIX;
        p.last_sample = g->samples;
        return;
    }
    if (g->samples == p.last_sample) return;   /* uut mõõtmist pole */
    float v = g->speed_kmh;
    int64_t t = g->sample_us;
    bool have_prev = p.last_sample != 0 && p.t_prev != 0;
    p.last_sample = g->samples;

    /* usutavus: mõõtmiste vahe ≤ 0,5 s ja kiirendus ≤ DASH_ACCEL_MAX_KMHS (nt GPS-i hüpe) */
    float dt = have_prev ? (t - p.t_prev) / 1e6f : 0;
    bool plausible = have_prev && dt > 0 && dt <= 0.5f && (v - p.v_prev) / dt <= DASH_ACCEL_MAX_KMHS;

    if (v < DASH_ACCEL_START_KMH) {
        if (!p.still_since) p.still_since = t;
    } else {
        p.still_since = 0;
    }

    switch (p.st) {
    case ST_NOFIX:
    case ST_WAIT:
        p.st = ST_WAIT;
        if (p.still_since && t - p.still_since >= 1000000) p.st = ST_READY;
        break;
    case ST_DONE:
        if (p.still_since && t - p.still_since >= 1000000) p.st = ST_READY;
        break;
    case ST_READY:
        if (v >= DASH_ACCEL_START_KMH) {
            if (plausible) {
                p.t_start = interp_t(p.t_prev, p.v_prev, t, v, DASH_ACCEL_START_KMH);
                p.t_split = 0;
                p.st = ST_RUN;
            } else {
                p.st = ST_WAIT;                      /* hüpe – ei ole päris start */
            }
        }
        break;
    case ST_RUN:
        if (!plausible) {
            p.st = ST_WAIT;                          /* GPS-i hüpe või katkestus mõõtmise ajal */
            break;
        }
        if (!p.t_split && v >= DASH_ACCEL_SPLIT) {
            p.t_split = interp_t(p.t_prev, p.v_prev, t, v, DASH_ACCEL_SPLIT);
            p.split = (p.t_split - p.t_start) / 1e6f;
        }
        if (v >= DASH_ACCEL_TARGET) {
            int64_t te = interp_t(p.t_prev, p.v_prev, t, v, DASH_ACCEL_TARGET);
            p.result = (te - p.t_start) / 1e6f;
            if (p.best <= 0 || p.result < p.best) p.best = p.result;
            memmove(&p.hist[1], &p.hist[0], sizeof(float) * (N_HIST - 1));
            p.hist[0] = p.result;
            if (p.n_hist < N_HIST) p.n_hist++;
            p.st = ST_DONE;
        } else if (v < DASH_ACCEL_START_KMH) {
            p.st = ST_READY;                         /* katkestatud */
        } else if (t - p.t_start > 30000000) {
            p.st = ST_WAIT;                          /* liiga kaua */
        }
        break;
    }
    p.t_prev = t;
    p.v_prev = v;
}

void accel_seed_history(const float *times, int n, float split)
{
    if (p.n_hist || n <= 0) return;
    if (n > N_HIST) n = N_HIST;
    for (int i = 0; i < n; i++) {
        p.hist[i] = times[i];
        if (p.best <= 0 || times[i] < p.best) p.best = times[i];
    }
    p.n_hist = n;
    p.result = times[0];
    p.split = split;
}

static void on_reset(lv_event_t *e)
{
    (void)e;
    p.best = 0;
    p.n_hist = 0;
    p.result = 0;
    p.split = 0;
    if (p.st == ST_DONE) p.st = ST_WAIT;
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
    lv_obj_t *title = dash_label(scr, 30, C_DIM, "0–100 km/h");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    p.time = dash_label(scr, 190, C_TEXT, "0.00");
    lv_obj_set_width(p.time, 700);
    lv_obj_set_style_text_align(p.time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.time, (W - 700) / 2, 70);

    p.status = dash_label(scr, 30, C_DIM, "");
    lv_obj_set_width(p.status, 800);
    lv_obj_set_style_text_align(p.status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.status, (W - 800) / 2, 300);
    p.status_col = -1;

    p.bar = dash_bar(scr, 760, 14, false);
    lv_obj_set_pos(p.bar, 40, 372);
    p.speed = dash_label(scr, 30, C_TEXT, "0");
    lv_obj_set_width(p.speed, 180);
    lv_obj_set_style_text_align(p.speed, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(p.speed, W - 220, 360);

    lv_obj_t *b1 = panel(scr, 12, 240, "0–" "60 km/h");
    p.split_v = dash_label(b1, 64, C_TEXT, "--");
    lv_obj_align(p.split_v, LV_ALIGN_BOTTOM_RIGHT, -14, -10);
    lv_obj_t *b2 = panel(scr, 264, 240, "PARIM");
    p.best_v = dash_label(b2, 64, C_YELLOW, "--");
    lv_obj_align(p.best_v, LV_ALIGN_BOTTOM_RIGHT, -14, -10);
    lv_obj_t *b3 = panel(scr, 516, 300, "VIIMASED");
    p.hist_l = dash_label(b3, 22, C_TEXT, "");
    lv_obj_set_pos(p.hist_l, 14, 30);

    dash_button(scr, "NULLI", 828, 430, 184, 156, on_reset);
}

static void update(const dash_state_t *s)
{
    char buf[96];
    float v = s->gps.fix ? s->gps.speed_kmh : 0;

    /* suur aeg: mõõtmise ajal jookseb, muidu viimane tulemus */
    if (p.st == ST_RUN) dash_fmt_time((s->now_us - p.t_start) / 1e6f, false, buf, 16);
    else dash_fmt_time(p.result, false, buf, 16);
    dash_set_text_if(p.time, p.t_time, sizeof(p.t_time), buf);

    static const char *TXT[] = { "OOTAN GPS SIGNAALI", "PEATU – ootan seismist",
                                 "VALMIS – sõida välja!", "MÕÕDAN…", "TULEMUS" };
    static const uint32_t COL[] = { 0xFFD21F, 0x7D8794, 0x22E06B, 0xFFD21F, 0x00D4FF };
    dash_set_text_if(p.status, p.t_status, sizeof(p.t_status), TXT[p.st]);
    if (p.status_col != p.st) {
        lv_obj_set_style_text_color(p.status, lv_color_hex(COL[p.st]), 0);
        lv_obj_set_style_text_color(p.time, p.st == ST_DONE ? C_ACCENT : C_TEXT, 0);
        p.status_col = (int8_t)p.st;
    }

    dash_bar_set(p.bar, v, 0, DASH_ACCEL_TARGET);
    snprintf(buf, 16, "%d km/h", (int)(v + 0.5f));
    dash_set_text_if(p.speed, p.t_speed, sizeof(p.t_speed), buf);

    if (p.split > 0) dash_fmt_time(p.split, false, buf, 16);
    else snprintf(buf, 16, "--");
    dash_set_text_if(p.split_v, p.t_split_v, sizeof(p.t_split_v), buf);
    if (p.best > 0) dash_fmt_time(p.best, false, buf, 16);
    else snprintf(buf, 16, "--");
    dash_set_text_if(p.best_v, p.t_best_v, sizeof(p.t_best_v), buf);

    buf[0] = 0;
    for (int i = 0; i < p.n_hist; i++) {
        char t[16];
        dash_fmt_time(p.hist[i], false, t, sizeof(t));
        size_t l = strlen(buf);
        snprintf(buf + l, sizeof(buf) - l, "%s%d.  %s s", i ? "\n" : "", i + 1, t);
    }
    if (!p.n_hist) snprintf(buf, sizeof(buf), "–");
    dash_set_text_if(p.hist_l, p.t_hist, sizeof(p.t_hist), buf);
}

const dash_page_t page_accel = { "0–100", 470, create, update, background, dash_need_gps, 938, 4, 16, 4, 856, 4 };
