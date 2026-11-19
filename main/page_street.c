/*
 * Vaade TÄNAV: vasakult üles kaarduv astmeline tahhomeeter, kiirus paremal üleval,
 * suur käik sinises kumas, sinine joon "sälguga", all segmentribad ja 6 väärtust.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dash_common.h"

#define N_SEG     84
#define SEG_W     6
#define SEG_H     42
#define TACH_X0   30
#define TACH_X1   770
#define TACH_TOP  48       /* tasase osa ülaserv */
#define RAMP_SEGS 22       /* mitu segmenti kaardub üles */
#define RAMP_H    104

#define LINE_Y    336      /* sinine joon */
#define NOTCH_Y   374
#define STACK_N   10
#define N_GLOW    5        /* 1 suur kast + 4 riba sälgu sees */
#define SUN_Y     NOTCH_Y  /* kuma keskpunkt */
#define SUN_R     360      /* kuma raadius */

#define G_LINE    C_ACCENT                 /* sama sinine nagu teistes vaadetes */
#define G_DIM     lv_color_hex(0x0F4C5C)
#define C_SEG_OFF lv_color_hex(0x252A30)

static const dash_tile_t GRID[] = DASH_STREET_GRID;
static const dash_tile_t ST_L1 = DASH_STREET_STACK_L1;
static const dash_tile_t ST_L2 = DASH_STREET_STACK_L2;
static const dash_tile_t ST_R  = DASH_STREET_STACK_R;
#define N_GRID (sizeof(GRID) / sizeof(GRID[0]))

typedef struct {
    dash_tile_t cfg;
    lv_obj_t   *seg[STACK_N], *val;
    int8_t      lit, alarm;
    char        last[16];
} vstack_t;

typedef struct {
    dash_tile_t cfg;
    lv_obj_t   *val, *lbl;
    char        last[16];
    int8_t      alarm;
} cell_t;

static struct {
    lv_obj_t *segs[N_SEG];
    int8_t    seg_st[N_SEG];          /* 0 väljas, 1.. värv */
    lv_obj_t *scale_lbl[13];
    int       rev_limit;
    lv_obj_t *gear, *glow[N_GLOW], *speed, *rpm;
    char      gear_txt[4], speed_txt[8], rpm_txt[8];
    int8_t    glow_st;
    vstack_t   st[3];
    cell_t    cells[N_GRID];
    dash_flags_t flags;
} p;

static lv_point_precise_t s_line_pts[6];
static lv_grad_dsc_t s_grad[N_GLOW][3];

/* segmendi i ülaserva y – esimesed RAMP_SEGS kaarduvad alt üles */
static int seg_y(int i)
{
    if (i >= RAMP_SEGS) return TACH_TOP;
    float t = (float)i / RAMP_SEGS;
    return TACH_TOP + (int)(RAMP_H * (1.0f - sinf(t * 1.5707963f)));
}

static int seg_x(int i)
{
    return TACH_X0 + i * (TACH_X1 - TACH_X0) / N_SEG;
}

static int seg_rpm(int i)
{
    return (i + 1) * DASH_RPM_MAX / N_SEG;
}

static void paint_off(void)
{
    for (int i = 0; i < N_SEG; i++) {
        lv_obj_set_style_bg_color(p.segs[i], seg_rpm(i) > p.rev_limit ? C_LIMIT : C_SEG_OFF, 0);
        p.seg_st[i] = 0;
    }
    for (int k = 0; k < 13; k++) {
        if (p.scale_lbl[k]) {
            lv_obj_set_style_text_color(p.scale_lbl[k], k * 1000 >= p.rev_limit ? C_RED : C_TEXT, 0);
        }
    }
}

static void stack_create(vstack_t *s, const dash_tile_t *cfg, lv_obj_t *parent, int x, bool scale)
{
    memset(s, 0, sizeof(*s));
    s->cfg = *cfg;
    s->lit = -1;
    s->alarm = -1;
    const int top = 398, sh = 13, gap = 4, w = 52;
    for (int k = 0; k < STACK_N; k++) {
        /* k = 0 on alumine */
        s->seg[k] = dash_box(parent, x, top + (STACK_N - 1 - k) * (sh + gap), w, sh, C_SEG_OFF, 2);
    }
    if (scale) {
        /* täisarvulised skaalamärgid vasakule */
        for (int v = (int)ceilf(cfg->bar_min); v <= (int)floorf(cfg->bar_max); v++) {
            float f = (v - cfg->bar_min) / (cfg->bar_max - cfg->bar_min);
            int y = top + (int)((1.0f - f) * (STACK_N * (sh + gap) - gap)) - 12;
            char t[6];
            snprintf(t, sizeof(t), "%d", v);
            lv_obj_t *l = dash_label(parent, 22, C_TEXT, t);
            lv_obj_set_width(l, 30);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_pos(l, x - 40, y);
        }
    }
    lv_obj_t *cap = dash_label(parent, 15, C_DIM, cfg->label);
    lv_obj_set_width(cap, 90);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(cap, x + w / 2 - 45, 568);
    s->val = dash_label(parent, 20, C_TEXT, "--");
    lv_obj_set_width(s->val, 90);
    lv_obj_set_style_text_align(s->val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s->val, x + w / 2 - 45, 370);
}

static void stack_update(vstack_t *s, const dash_state_t *st)
{
    char buf[16];
    bool valid = st->d.valid[s->cfg.sig];
    float v = st->d.v[s->cfg.sig] * s->cfg.scale;
    if (valid) snprintf(buf, sizeof(buf), "%.*f%s", s->cfg.decimals, v, s->cfg.unit);
    else snprintf(buf, sizeof(buf), "--");
    dash_set_text_if(s->val, s->last, sizeof(s->last), buf);

    float f = valid ? (v - s->cfg.bar_min) / (s->cfg.bar_max - s->cfg.bar_min) : 0;
    int8_t lit = (int8_t)(f <= 0 ? 0 : f >= 1 ? STACK_N : ceilf(f * STACK_N));
    int8_t al = dash_sig_alarm(st, s->cfg.sig);
    if (lit != s->lit || al != s->alarm) {
        for (int k = 0; k < STACK_N; k++) {
            lv_color_t c = k >= lit ? C_SEG_OFF : al ? C_RED : k >= STACK_N - 2 ? C_YELLOW : G_LINE;
            lv_obj_set_style_bg_color(s->seg[k], c, 0);
        }
        lv_obj_set_style_text_color(s->val, al ? C_RED : C_TEXT, 0);
        s->lit = lit;
        s->alarm = al;
    }
}

static void create(lv_obj_t *scr)
{
    p.rev_limit = DASH_RPM_MAX - 500;
    dash_flags_create(&p.flags, scr, W / 2, 6);

    /* kuma: ümmargune "loojuv päike", keskpunkt sälgu põhjas */
    const int strip_h = (NOTCH_Y - LINE_Y) / (N_GLOW - 1);
    int glx[N_GLOW], gly[N_GLOW];
    glx[0] = 0;
    gly[0] = 36;
    p.glow[0] = dash_box(scr, glx[0], gly[0], W, LINE_Y - 36, C_BG, 0);
    for (int k = 1; k < N_GLOW; k++) {
        /* sälgu sisse astmelised ribad, mis järgivad kaldjooni */
        int y0 = LINE_Y + (k - 1) * strip_h;
        int inset = 300 + (k * 2 - 1) * 50 / ((N_GLOW - 1) * 2);
        glx[k] = inset;
        gly[k] = y0;
        p.glow[k] = dash_box(scr, inset, y0, W - 2 * inset, k == N_GLOW - 1 ? NOTCH_Y - y0 : strip_h, C_BG, 0);
    }
    static const uint32_t GC[3] = { 0x00A6D6, 0xA01C1C, 0x2F7BFF };
    for (int k = 0; k < N_GLOW; k++) {
        int rx = W / 2 - glx[k], ry = SUN_Y - gly[k];
        for (int c = 0; c < 3; c++) {
            lv_color_t col[4] = { lv_color_hex(GC[c]), lv_color_hex(GC[c]), lv_color_hex(GC[c]), lv_color_hex(GC[c]) };
            static const lv_opa_t opa[4] = { 255, 150, 45, 0 };
            static const uint8_t fr[4] = { 0, 90, 175, 255 };
            lv_grad_dsc_t *g = &s_grad[k][c];
            lv_grad_init_stops(g, col, opa, fr, 4);
            lv_grad_radial_init(g, rx, ry, rx + SUN_R, ry, LV_GRAD_EXTEND_PAD);
        }
        lv_obj_set_style_bg_grad(p.glow[k], &s_grad[k][0], 0);
    }
    p.glow_st = 0;

    const lv_point_precise_t pts[6] = {
        { 0, LINE_Y }, { 300, LINE_Y }, { 350, NOTCH_Y }, { W - 350, NOTCH_Y }, { W - 300, LINE_Y }, { W, LINE_Y },
    };
    memcpy(s_line_pts, pts, sizeof(pts));
    lv_obj_t *ln = lv_line_create(scr);
    lv_line_set_points(ln, s_line_pts, 6);
    lv_obj_set_style_line_width(ln, 4, 0);
    lv_obj_set_style_line_color(ln, G_LINE, 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
    lv_obj_set_clickable(ln, false);

    /* tahhomeeter */
    for (int i = 0; i < N_SEG; i++) {
        p.segs[i] = dash_box(scr, seg_x(i), seg_y(i), SEG_W, SEG_H, C_SEG_OFF, 1);
    }
    for (int k = 1; k * 1000 <= DASH_RPM_MAX && k < 13; k++) {
        int i = k * 1000 * N_SEG / DASH_RPM_MAX - 1;
        char t[4];
        snprintf(t, sizeof(t), "%d", k);
        lv_obj_t *l = dash_label(scr, 26, C_TEXT, t);
        lv_obj_set_width(l, 30);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(l, seg_x(i) + SEG_W / 2 - 15 + (i < RAMP_SEGS ? 12 : 0), seg_y(i) + SEG_H + 4);
        p.scale_lbl[k] = l;
    }
    paint_off();

    /* kiirus + pöörded paremal üleval */
    p.speed = dash_label(scr, 110, C_TEXT, "0");
    lv_obj_set_width(p.speed, 200);
    lv_obj_set_style_text_align(p.speed, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(p.speed, W - 268, 34);
    lv_obj_t *u = dash_label(scr, 20, C_DIM, "km/h");
    lv_obj_set_pos(u, W - 60, 112);
    p.rpm = dash_label(scr, 30, C_DIM, "0");
    lv_obj_set_width(p.rpm, 160);
    lv_obj_set_style_text_align(p.rpm, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(p.rpm, W - 228, 162);
    lv_obj_t *ru = dash_label(scr, 17, C_DIM, "rpm");
    lv_obj_set_pos(ru, W - 60, 174);

    /* käik */
    p.gear = dash_label(scr, 230, C_TEXT, "-");
    lv_obj_set_width(p.gear, 300);
    lv_obj_set_style_text_align(p.gear, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.gear, W / 2 - 150, 118);

    /* alumine osa: eraldusjooned, segmentribad, ruudustik */
    dash_box(scr, 236, 396, 2, 190, G_DIM, 0);
    dash_box(scr, W - 238, 396, 2, 190, G_DIM, 0);
    stack_create(&p.st[0], &ST_L1, scr, 40, false);
    stack_create(&p.st[1], &ST_L2, scr, 140, false);
    stack_create(&p.st[2], &ST_R, scr, W - 120, true);

    const int gx = 250, gw = W - 500, cw = gw / 3, ch = 96, gy = 392;
    for (size_t i = 0; i < N_GRID; i++) {
        cell_t *c = &p.cells[i];
        c->cfg = GRID[i];
        c->alarm = -1;
        lv_obj_t *cell = dash_box(scr, gx + (i % 3) * cw, gy + (i / 3) * ch, cw, ch, C_BG, 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
        c->val = dash_value_with_unit(cell, 46, C_TEXT, c->cfg.unit, -30, 6);
        lv_label_set_text(c->val, "--");
        c->lbl = dash_label(cell, 15, C_DIM, c->cfg.label);
        lv_obj_align(c->lbl, LV_ALIGN_BOTTOM_MID, 0, -8);
    }
}

static void update(const dash_state_t *s)
{
    char buf[16];
    float rpm = s->rpm;
    if (s->rev_limit != p.rev_limit) {
        p.rev_limit = s->rev_limit;
        paint_off();
    }

    int lit = (int)(rpm / DASH_RPM_MAX * N_SEG + 0.5f);
    for (int i = 0; i < N_SEG; i++) {
        int r = seg_rpm(i);
        int8_t st = 0;
        if (i < lit) {
            if (s->flashing) st = s->blink_fast ? 4 : 0;
            else st = r > p.rev_limit ? 3 : r >= DASH_SHIFT_START ? 2 : 1;
        }
        if (st != p.seg_st[i]) {
            lv_color_t c = st == 1 ? G_LINE : st == 2 ? C_YELLOW : st == 3 ? C_RED : st == 4 ? C_BLUE
                         : r > p.rev_limit ? C_LIMIT : C_SEG_OFF;
            lv_obj_set_style_bg_color(p.segs[i], c, 0);
            p.seg_st[i] = st;
        }
    }

    dash_gear_text(s, buf, sizeof(buf));
    dash_set_text_if(p.gear, p.gear_txt, sizeof(p.gear_txt), buf);
    int8_t gs = s->flashing ? (s->blink_fast ? 2 : 1) : 0;
    if (gs != p.glow_st) {
        /* 0 sinine (tavaline), 1 punane, 2 tugev sinine (shift) */
        int c = gs == 2 ? 2 : gs == 1 ? 1 : 0;
        for (int k = 0; k < N_GLOW; k++) lv_obj_set_style_bg_grad(p.glow[k], &s_grad[k][c], 0);
        p.glow_st = gs;
    }
    snprintf(buf, sizeof(buf), "%d", s->d.valid[SIG_SPEED] ? (int)(s->d.v[SIG_SPEED] + 0.5f) : 0);
    dash_set_text_if(p.speed, p.speed_txt, sizeof(p.speed_txt), buf);
    snprintf(buf, sizeof(buf), "%d", (int)rpm);
    dash_set_text_if(p.rpm, p.rpm_txt, sizeof(p.rpm_txt), buf);

    for (int i = 0; i < 3; i++) stack_update(&p.st[i], s);
    for (size_t i = 0; i < N_GRID; i++) {
        cell_t *c = &p.cells[i];
        dash_fmt(s, &c->cfg, buf, sizeof(buf));
        dash_set_text_if(c->val, c->last, sizeof(c->last), buf);
        int8_t al = dash_sig_alarm(s, c->cfg.sig) && s->blink;
        if (al != c->alarm) {
            lv_obj_set_style_text_color(c->val, al ? C_RED : C_TEXT, 0);
            lv_obj_set_style_text_color(c->lbl, al ? C_RED : C_DIM, 0);
            c->alarm = al;
        }
    }
    dash_flags_update(&p.flags, s);
}

const dash_page_t page_street = { "TÄNAV", 400, create, update, NULL, NULL, 846, 6, 16, 4 };
