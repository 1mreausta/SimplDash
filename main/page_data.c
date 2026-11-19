/*
 * Vaade ANDMED: kõik põhiväärtused tabelina koos min/max tippudega
 * (tipud loetakse alati, ka siis kui vaade pole avatud; nupp NULLI tühjendab).
 */
#include <stdio.h>
#include <string.h>
#include "dash_common.h"

static const dash_tile_t ROWS[] = DASH_DATA_ROWS;
#define N_ROWS (sizeof(ROWS) / sizeof(ROWS[0]))
#define PER_COL ((N_ROWS + 1) / 2)

typedef struct {
    lv_obj_t *val, *min, *max;
    char      t_val[16], t_min[16], t_max[16];
    float     lo, hi;
    bool      have;
    int8_t    alarm;
} row_t;

static struct {
    row_t rows[N_ROWS];
} p;

static void reset_peaks(void)
{
    for (size_t i = 0; i < N_ROWS; i++) p.rows[i].have = false;
}

static void on_reset(lv_event_t *e)
{
    (void)e;
    reset_peaks();
}

static void create(lv_obj_t *scr)
{
    lv_obj_t *t = dash_label(scr, 40, C_TEXT, "ANDMED");
    lv_obj_set_pos(t, 16, 6);
    lv_obj_t *st = dash_label(scr, 17, C_DIM, "min / max alates käivitusest või nullimisest");
    lv_obj_set_pos(st, 150, 24);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 170, 44);
    lv_obj_set_pos(btn, W - 290, 6);
    lv_obj_set_style_bg_color(btn, C_PANEL, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, C_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, C_LINE, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_t *bl = dash_label(btn, 22, C_TEXT, "NULLI TIPUD");
    lv_obj_center(bl);
    lv_obj_add_event_cb(btn, on_reset, LV_EVENT_CLICKED, NULL);

    const int col_w = 494, row_h = 41, top = 92;
    for (int c = 0; c < 2; c++) {
        int x0 = 12 + c * (col_w + 12);
        lv_obj_t *h1 = dash_label(scr, 15, C_DIM, "PRAEGU");
        lv_obj_set_width(h1, 120);
        lv_obj_set_style_text_align(h1, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(h1, x0 + 200, 66);
        lv_obj_t *h2 = dash_label(scr, 15, C_DIM, "MIN");
        lv_obj_set_width(h2, 80);
        lv_obj_set_style_text_align(h2, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(h2, x0 + 320, 66);
        lv_obj_t *h3 = dash_label(scr, 15, C_DIM, "MAX");
        lv_obj_set_width(h3, 80);
        lv_obj_set_style_text_align(h3, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(h3, x0 + 404, 66);
    }

    for (size_t i = 0; i < N_ROWS; i++) {
        int c = i / PER_COL, k = i % PER_COL;
        int x0 = 12 + c * (col_w + 12), y = top + k * row_h;
        row_t *r = &p.rows[i];
        r->alarm = -1;
        lv_obj_t *bg = dash_box(scr, x0, y, col_w, row_h - 3, k % 2 ? C_BG : C_PANEL, 6);
        lv_obj_t *l = dash_label(bg, 22, C_TEXT, ROWS[i].label);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_t *u = dash_label(bg, 15, C_DIM, ROWS[i].unit);
        lv_obj_align(u, LV_ALIGN_LEFT_MID, 150, 2);

        r->val = dash_label(bg, 32, C_TEXT, "--");
        lv_obj_set_width(r->val, 120);
        lv_obj_set_style_text_align(r->val, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(r->val, LV_ALIGN_LEFT_MID, 200, 0);
        r->min = dash_label(bg, 24, C_ACCENT, "--");
        lv_obj_set_width(r->min, 80);
        lv_obj_set_style_text_align(r->min, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(r->min, LV_ALIGN_LEFT_MID, 320, 1);
        r->max = dash_label(bg, 24, C_YELLOW, "--");
        lv_obj_set_width(r->max, 80);
        lv_obj_set_style_text_align(r->max, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(r->max, LV_ALIGN_LEFT_MID, 404, 1);
    }
}

static void background(const dash_state_t *s)
{
    if (s->stale) return;
    for (size_t i = 0; i < N_ROWS; i++) {
        if (!s->d.valid[ROWS[i].sig]) continue;
        row_t *r = &p.rows[i];
        float v = s->d.v[ROWS[i].sig] * ROWS[i].scale;
        if (!r->have) { r->lo = r->hi = v; r->have = true; }
        if (v < r->lo) r->lo = v;
        if (v > r->hi) r->hi = v;
    }
}

static void update(const dash_state_t *s)
{
    char buf[16];
    for (size_t i = 0; i < N_ROWS; i++) {
        row_t *r = &p.rows[i];
        const dash_tile_t *c = &ROWS[i];
        dash_fmt(s, c, buf, sizeof(buf));
        dash_set_text_if(r->val, r->t_val, sizeof(r->t_val), buf);
        if (r->have) snprintf(buf, sizeof(buf), "%.*f", c->decimals, r->lo);
        else snprintf(buf, sizeof(buf), "--");
        dash_set_text_if(r->min, r->t_min, sizeof(r->t_min), buf);
        if (r->have) snprintf(buf, sizeof(buf), "%.*f", c->decimals, r->hi);
        else snprintf(buf, sizeof(buf), "--");
        dash_set_text_if(r->max, r->t_max, sizeof(r->t_max), buf);
        int8_t al = dash_sig_alarm(s, c->sig) && s->blink;
        if (al != r->alarm) {
            lv_obj_set_style_text_color(r->val, al ? C_RED : C_TEXT, 0);
            r->alarm = al;
        }
    }
}

const dash_page_t page_data = { "ANDMED", 2, create, update, background, NULL, 938, 4, 560, 12, 652, 14 };
