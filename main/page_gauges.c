/*
 * Vaade KELLAD: kaks ümarat kella (vasakul pöörded + käik, paremal kiirus + ülerõhk),
 * nende vahel põhinäidud ja kaks vertikaalset riba, all 4 näidikut.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dash_common.h"

#define R      186          /* kella raadius */
#define CY     318
#define CX_L   212
#define CX_R   (W - 212)
#define SWEEP  270          /* kella kaare ulatus kraadides */

static const dash_tile_t CENTER[] = DASH_GAUGES_CENTER;
static const dash_tile_t BOTTOM[] = DASH_GAUGES_BOTTOM;
static const dash_tile_t BAR_L = DASH_GAUGES_BAR_LEFT;
static const dash_tile_t BAR_R = DASH_GAUGES_BAR_RIGHT;
#define N_CENTER (sizeof(CENTER) / sizeof(CENTER[0]))
#define N_BOTTOM (sizeof(BOTTOM) / sizeof(BOTTOM[0]))

typedef struct {
    lv_obj_t *arc, *scale, *band;
    lv_scale_section_t *red;
    int       max;
    int32_t   last;
    int8_t    col;
} gauge_t;

typedef struct {
    dash_tile_t cfg;
    lv_obj_t   *val, *lbl, *unit;
    char        last[16];
    int8_t      alarm;
} row_t;

typedef struct {
    dash_tile_t cfg;
    lv_obj_t   *bar, *val;
    char        last[16];
    int8_t      alarm;
} vbar_t;

static struct {
    gauge_t tach, spd;
    int     rev_limit;
    lv_obj_t *gear, *rpm, *speed, *boost;
    char    gear_txt[4], rpm_txt[8], speed_txt[8], boost_txt[12];
    int8_t  gear_blue;
    row_t   rows[N_CENTER];
    vbar_t  bl, br;
    dash_tile_obj_t bottom[N_BOTTOM];
    dash_flags_t flags;
} p;

static lv_style_t st_red_ind, st_red_items;

static const char *TACH_TXT[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", NULL };

/* Kaar ilma nuputa, rotation 135° → 0 vasakul all, max paremal all */
static lv_obj_t *mk_arc(lv_obj_t *parent, int cx, int cy, int r, int width, lv_color_t bg, lv_color_t ind)
{
    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, 2 * r, 2 * r);
    lv_obj_set_pos(a, cx - r, cy - r);
    lv_arc_set_rotation(a, 135);
    lv_arc_set_bg_angles(a, 0, SWEEP);
    lv_arc_set_range(a, 0, 1000);
    lv_arc_set_value(a, 0);
    lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, bg, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, ind, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
    lv_obj_set_clickable(a, false);
    lv_obj_set_scrollable(a, false);
    return a;
}

static void gauge_create(gauge_t *g, lv_obj_t *parent, int cx, int max, int ticks, int major,
                         const char **txt, int label_px)
{
    g->max = max;
    g->last = -1;
    g->col = -1;

    lv_obj_t *disc = dash_box(parent, cx - R, CY - R, 2 * R, 2 * R, lv_color_hex(0x07090C), LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_color(disc, lv_color_hex(0x232A33), 0);
    lv_obj_set_style_border_width(disc, 3, 0);

    /* punane tsoon (ainult tahhomeetril nähtav) välisservas */
    g->band = mk_arc(parent, cx, CY, R - 8, 6, lv_color_hex(0x8A1414), C_BG);
    lv_obj_set_style_arc_opa(g->band, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_hidden(g->band, true);

    g->arc = mk_arc(parent, cx, CY, R - 16, 16, C_TRACK, C_ACCENT);

    lv_obj_t *s = lv_scale_create(parent);
    const int sr = R - 38;
    lv_obj_set_size(s, 2 * sr, 2 * sr);
    lv_obj_set_pos(s, cx - sr, CY - sr);
    lv_scale_set_mode(s, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(s, 0, max);
    lv_scale_set_total_tick_count(s, ticks);
    lv_scale_set_major_tick_every(s, major);
    lv_scale_set_label_show(s, true);
    if (txt) lv_scale_set_text_src(s, txt);
    lv_scale_set_angle_range(s, SWEEP);
    lv_scale_set_rotation(s, 135);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_set_style_arc_width(s, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_length(s, 14, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(s, 3, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(s, C_TEXT, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(s, C_TEXT, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(s, dash_font(label_px), LV_PART_INDICATOR);
    lv_obj_set_style_pad_radial(s, 4, LV_PART_INDICATOR);
    lv_obj_set_style_length(s, 7, LV_PART_ITEMS);
    lv_obj_set_style_line_width(s, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(s, C_DIM, LV_PART_ITEMS);
    lv_obj_set_clickable(s, false);
    lv_obj_set_scrollable(s, false);
    g->scale = s;
    g->red = NULL;
}

static void gauge_set(gauge_t *g, float v, lv_color_t col, int8_t col_id)
{
    int32_t pm = (int32_t)(v / g->max * 1000 + 0.5f);
    pm = pm < 0 ? 0 : pm > 1000 ? 1000 : pm;
    if (pm != g->last) {
        lv_arc_set_value(g->arc, pm);
        g->last = pm;
    }
    if (col_id != g->col) {
        lv_obj_set_style_arc_color(g->arc, col, LV_PART_INDICATOR);
        g->col = col_id;
    }
}

static void tach_set_limit(int limit)
{
    gauge_t *g = &p.tach;
    if (limit >= g->max) {
        lv_obj_set_hidden(g->band, true);
        return;
    }
    if (!g->red) {
        g->red = lv_scale_add_section(g->scale);
        lv_scale_set_section_style_indicator(g->scale, g->red, &st_red_ind);
        lv_scale_set_section_style_items(g->scale, g->red, &st_red_items);
    }
    lv_scale_set_section_range(g->scale, g->red, limit, g->max);
    lv_arc_set_bg_angles(g->band, (int32_t)((float)limit / g->max * SWEEP), SWEEP);
    lv_obj_set_hidden(g->band, false);
}

static lv_obj_t *centered_label(lv_obj_t *parent, int px, lv_color_t col, int x, int y, int w)
{
    lv_obj_t *l = dash_label(parent, px, col, "--");
    lv_obj_set_width(l, w);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

static void vbar_create(vbar_t *b, const dash_tile_t *cfg, lv_obj_t *parent, int x)
{
    memset(b, 0, sizeof(*b));
    b->cfg = *cfg;
    b->alarm = -1;
    lv_obj_t *cap = centered_label(parent, 15, C_DIM, x - 25, 118, 60);
    lv_label_set_text(cap, cfg->label);
    b->bar = dash_bar(parent, 10, 336, true);
    lv_obj_set_pos(b->bar, x, 142);
    b->val = centered_label(parent, 20, C_TEXT, x - 25, 484, 60);
}

static void vbar_update(vbar_t *b, const dash_state_t *s)
{
    char buf[16];
    bool valid = s->d.valid[b->cfg.sig];
    float v = s->d.v[b->cfg.sig] * b->cfg.scale;
    if (valid) snprintf(buf, sizeof(buf), "%.*f%s", b->cfg.decimals, v, b->cfg.unit);
    else snprintf(buf, sizeof(buf), "--");
    dash_set_text_if(b->val, b->last, sizeof(b->last), buf);
    if (valid) dash_bar_set(b->bar, v, b->cfg.bar_min, b->cfg.bar_max);
    int8_t al = dash_sig_alarm(s, b->cfg.sig);
    if (al != b->alarm) {
        lv_obj_set_style_bg_color(b->bar, al ? C_RED : C_ACCENT, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(b->val, al ? C_RED : C_TEXT, 0);
        b->alarm = al;
    }
}

static void create(lv_obj_t *scr)
{
    lv_style_init(&st_red_ind);
    lv_style_set_line_color(&st_red_ind, C_RED);
    lv_style_set_text_color(&st_red_ind, C_RED);
    lv_style_init(&st_red_items);
    lv_style_set_line_color(&st_red_items, C_RED);

    dash_flags_create(&p.flags, scr, W / 2, 10);

    /* Tahhomeeter: 200 rpm sammuga kriipsud, number iga 1000 järel */
    int tach_max = DASH_RPM_MAX;
    gauge_create(&p.tach, scr, CX_L, tach_max, tach_max / 200 + 1, 5, TACH_TXT, 26);
    lv_obj_t *tl = dash_label(scr, 15, C_DIM, "x1000 rpm");
    lv_obj_set_pos(tl, CX_L - 30, CY + R - 40);

    /* Spidomeeter: 10 km/h kriipsud, number iga 20 järel */
    gauge_create(&p.spd, scr, CX_R, DASH_SPEED_MAX, DASH_SPEED_MAX / 10 + 1, 2, NULL, 20);
    lv_obj_t *sl = dash_label(scr, 15, C_DIM, "km/h");
    lv_obj_set_pos(sl, CX_R - 15, CY + R - 40);

    p.gear = centered_label(scr, 150, C_TEXT, CX_L - 90, CY - 118, 180);
    p.rpm  = centered_label(scr, 36, C_TEXT, CX_L - 80, CY + 52, 160);
    lv_obj_t *rl = centered_label(scr, 15, C_DIM, CX_L - 40, CY + 94, 80);
    lv_label_set_text(rl, "RPM");

    p.speed = centered_label(scr, 120, C_TEXT, CX_R - 110, CY - 92, 220);
    lv_obj_t *bl = centered_label(scr, 15, C_DIM, CX_R - 60, CY + 50, 120);
    lv_label_set_text(bl, "ÜLERÕHK bar");
    p.boost = centered_label(scr, 32, C_TEXT, CX_R - 80, CY + 68, 160);

    /* Keskmine tulp */
    const int col_x = 432, col_w = 162, row_h = 66, top = 142;
    for (size_t i = 0; i < N_CENTER; i++) {
        row_t *r = &p.rows[i];
        r->cfg = CENTER[i];
        r->alarm = -1;
        int y = top + i * (row_h + 6);
        r->lbl = dash_label(scr, 17, C_DIM, r->cfg.label);
        lv_obj_set_pos(r->lbl, col_x, y + 4);
        r->unit = dash_label(scr, 15, C_DIM, r->cfg.unit);
        lv_obj_set_pos(r->unit, col_x, y + 26);
        r->val = dash_label(scr, 46, C_TEXT, "--");
        lv_obj_set_width(r->val, 110);
        lv_obj_set_style_text_align(r->val, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(r->val, col_x + col_w - 110, y + 2);
        if (i + 1 < N_CENTER) dash_box(scr, col_x, y + row_h + 2, col_w, 1, C_LINE, 0);
    }
    vbar_create(&p.bl, &BAR_L, scr, 412);
    vbar_create(&p.br, &BAR_R, scr, W - 422);

    /* Alumine rida */
    const int bw = 236, gap = 12;
    int bx = (W - (int)N_BOTTOM * bw - ((int)N_BOTTOM - 1) * gap) / 2;
    for (size_t i = 0; i < N_BOTTOM; i++) {
        dash_tile_create(&p.bottom[i], &BOTTOM[i], scr, bx + i * (bw + gap), 524, bw, 66, 36);
    }

    p.rev_limit = DASH_RPM_MAX - 500;
    tach_set_limit(p.rev_limit);
}

static void update(const dash_state_t *s)
{
    char buf[16];
    if (s->rev_limit != p.rev_limit) {
        p.rev_limit = s->rev_limit;
        tach_set_limit(p.rev_limit);
    }

    /* tahhomeetri värv: roheline → kollane → punane → vilkuv sinine */
    float rpm = s->rpm;
    int8_t cid = s->flashing ? (s->blink_fast ? 3 : 2) : rpm >= DASH_SHIFT_START ? 1 : 0;
    static const uint32_t TCOL[] = { 0x00D4FF, 0xFFD21F, 0xFF2D2D, 0x2F7BFF };
    gauge_set(&p.tach, rpm, lv_color_hex(TCOL[cid]), cid);

    float spd = s->d.valid[SIG_SPEED] ? s->d.v[SIG_SPEED] : 0;
    gauge_set(&p.spd, spd, C_ACCENT, 0);

    dash_gear_text(s, buf, sizeof(buf));
    dash_set_text_if(p.gear, p.gear_txt, sizeof(p.gear_txt), buf);
    if (s->flashing != p.gear_blue) {
        lv_obj_set_style_text_color(p.gear, s->flashing ? C_BLUE : C_TEXT, 0);
        p.gear_blue = s->flashing;
    }
    snprintf(buf, sizeof(buf), "%d", (int)rpm);
    dash_set_text_if(p.rpm, p.rpm_txt, sizeof(p.rpm_txt), buf);
    snprintf(buf, sizeof(buf), "%d", (int)(spd + 0.5f));
    dash_set_text_if(p.speed, p.speed_txt, sizeof(p.speed_txt), buf);
    if (s->d.valid[SIG_BOOST_BAR]) snprintf(buf, sizeof(buf), "%.2f", s->d.v[SIG_BOOST_BAR]);
    else snprintf(buf, sizeof(buf), "--");
    dash_set_text_if(p.boost, p.boost_txt, sizeof(p.boost_txt), buf);

    for (size_t i = 0; i < N_CENTER; i++) {
        row_t *r = &p.rows[i];
        dash_fmt(s, &r->cfg, buf, sizeof(buf));
        dash_set_text_if(r->val, r->last, sizeof(r->last), buf);
        int8_t al = dash_sig_alarm(s, r->cfg.sig) && s->blink;
        if (al != r->alarm) {
            lv_obj_set_style_text_color(r->val, al ? C_RED : C_TEXT, 0);
            lv_obj_set_style_text_color(r->lbl, al ? C_RED : C_DIM, 0);
            r->alarm = al;
        }
    }
    vbar_update(&p.bl, s);
    vbar_update(&p.br, s);
    for (size_t i = 0; i < N_BOTTOM; i++) dash_tile_update(&p.bottom[i], s);
    dash_flags_update(&p.flags, s);
}

const dash_page_t page_gauges = { "KELLAD", 52, create, update, NULL, NULL, 938, 4, 16, 4, 856, 4 };
