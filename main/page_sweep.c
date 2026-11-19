/*
 * Vaade KAAR: suur kaartahhomeeter keskel, kiirus ja pöörded selle sees,
 * käik all keskel, 3 + 3 näidikut külgedel.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dash_common.h"

#define CX     (W / 2)
#define CY     408
#define R      284
#define ARC_W  44
#define ROT    150      /* kaar algab vasakult alt */
#define SWEEP  240

static const dash_tile_t TL[] = DASH_SWEEP_LEFT;
static const dash_tile_t TR[] = DASH_SWEEP_RIGHT;
#define N_TL (sizeof(TL) / sizeof(TL[0]))
#define N_TR (sizeof(TR) / sizeof(TR[0]))

static struct {
    lv_obj_t *arc, *band, *scale;
    lv_scale_section_t *red;
    int32_t   last;
    int8_t    col;
    int       rev_limit;
    lv_obj_t *speed, *rpm, *gear, *gear_box;
    char      speed_txt[8], rpm_txt[8], gear_txt[4];
    int8_t    gear_state;
    dash_tile_obj_t tiles[N_TL + N_TR];
    dash_flags_t flags;
} p;

static lv_style_t st_red_ind, st_red_items;
static const char *TXT[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", NULL };

static lv_obj_t *mk_arc(lv_obj_t *parent, int r, int width, lv_color_t bg, lv_color_t ind)
{
    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, 2 * r, 2 * r);
    lv_obj_set_pos(a, CX - r, CY - r);
    lv_arc_set_rotation(a, ROT);
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

static void set_limit(int limit)
{
    if (limit >= DASH_RPM_MAX) {
        lv_obj_set_hidden(p.band, true);
        return;
    }
    if (!p.red) {
        p.red = lv_scale_add_section(p.scale);
        lv_scale_set_section_style_indicator(p.scale, p.red, &st_red_ind);
        lv_scale_set_section_style_items(p.scale, p.red, &st_red_items);
    }
    lv_scale_set_section_range(p.scale, p.red, limit, DASH_RPM_MAX);
    lv_arc_set_bg_angles(p.band, (int32_t)((float)limit / DASH_RPM_MAX * SWEEP), SWEEP);
    lv_obj_set_hidden(p.band, false);
}

static void create(lv_obj_t *scr)
{
    lv_style_init(&st_red_ind);
    lv_style_set_line_color(&st_red_ind, C_RED);
    lv_style_set_text_color(&st_red_ind, C_RED);
    lv_style_init(&st_red_items);
    lv_style_set_line_color(&st_red_items, C_RED);

    dash_flags_create(&p.flags, scr, W / 2, 10);

    /* kaar: välisservas punane tsoon, siis näidukaar */
    p.band = mk_arc(scr, R + 8, 6, lv_color_hex(0x8A1414), C_BG);
    lv_obj_set_style_arc_opa(p.band, LV_OPA_TRANSP, LV_PART_INDICATOR);
    p.arc = mk_arc(scr, R, ARC_W, C_TRACK, C_ACCENT);

    lv_obj_t *s = lv_scale_create(scr);
    const int sr = R - ARC_W - 6;
    lv_obj_set_size(s, 2 * sr, 2 * sr);
    lv_obj_set_pos(s, CX - sr, CY - sr);
    lv_scale_set_mode(s, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(s, 0, DASH_RPM_MAX);
    lv_scale_set_total_tick_count(s, DASH_RPM_MAX / 250 + 1);
    lv_scale_set_major_tick_every(s, 4);
    lv_scale_set_label_show(s, true);
    lv_scale_set_text_src(s, TXT);
    lv_scale_set_angle_range(s, SWEEP);
    lv_scale_set_rotation(s, ROT);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_set_style_arc_width(s, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_length(s, 16, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(s, 4, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(s, C_TEXT, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(s, C_TEXT, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(s, dash_font(34), LV_PART_INDICATOR);
    lv_obj_set_style_pad_radial(s, 6, LV_PART_INDICATOR);
    lv_obj_set_style_length(s, 8, LV_PART_ITEMS);
    lv_obj_set_style_line_width(s, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(s, C_DIM, LV_PART_ITEMS);
    lv_obj_set_clickable(s, false);
    lv_obj_set_scrollable(s, false);
    p.scale = s;

    /* keskel: kiirus, ühik, pöörded */
    p.speed = dash_label(scr, 170, C_TEXT, "0");
    lv_obj_set_width(p.speed, 340);
    lv_obj_set_style_text_align(p.speed, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.speed, CX - 170, 206);
    lv_obj_t *u = dash_label(scr, 20, C_DIM, "km/h");
    lv_obj_set_width(u, 100);
    lv_obj_set_style_text_align(u, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(u, CX - 50, 386);
    p.rpm = dash_label(scr, 40, C_DIM, "0");
    lv_obj_set_width(p.rpm, 200);
    lv_obj_set_style_text_align(p.rpm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(p.rpm, CX - 100, 412);

    /* käik */
    p.gear_box = dash_box(scr, CX - 60, 470, 120, 120, C_PANEL, 14);
    lv_obj_set_style_border_color(p.gear_box, C_LINE, 0);
    lv_obj_set_style_border_width(p.gear_box, 2, 0);
    p.gear = dash_label(p.gear_box, 110, C_TEXT, "-");
    lv_obj_center(p.gear);
    p.gear_state = -1;

    /* külgede näidikud */
    const int tw = 200, th = 140, gap = 12, top = 130;
    for (size_t i = 0; i < N_TL; i++) {
        dash_tile_create(&p.tiles[i], &TL[i], scr, 12, top + i * (th + gap), tw, th, 64);
    }
    for (size_t i = 0; i < N_TR; i++) {
        dash_tile_create(&p.tiles[N_TL + i], &TR[i], scr, W - 12 - tw, top + i * (th + gap), tw, th, 64);
    }

    p.last = -1;
    p.col = -1;
    p.rev_limit = DASH_RPM_MAX - 500;
    set_limit(p.rev_limit);
}

static void update(const dash_state_t *s)
{
    char buf[16];
    if (s->rev_limit != p.rev_limit) {
        p.rev_limit = s->rev_limit;
        set_limit(p.rev_limit);
    }

    float rpm = s->rpm;
    int32_t pm = (int32_t)(rpm / DASH_RPM_MAX * 1000 + 0.5f);
    pm = pm < 0 ? 0 : pm > 1000 ? 1000 : pm;
    if (pm != p.last) {
        lv_arc_set_value(p.arc, pm);
        p.last = pm;
    }
    int8_t cid = s->flashing ? (s->blink_fast ? 3 : 2) : rpm >= DASH_SHIFT_START ? 1 : 0;
    if (cid != p.col) {
        static const uint32_t COL[] = { 0x00D4FF, 0xFFD21F, 0xFF2D2D, 0x2F7BFF };
        lv_obj_set_style_arc_color(p.arc, lv_color_hex(COL[cid]), LV_PART_INDICATOR);
        p.col = cid;
    }

    snprintf(buf, sizeof(buf), "%d", s->d.valid[SIG_SPEED] ? (int)(s->d.v[SIG_SPEED] + 0.5f) : 0);
    dash_set_text_if(p.speed, p.speed_txt, sizeof(p.speed_txt), buf);
    snprintf(buf, sizeof(buf), "%d", (int)rpm);
    dash_set_text_if(p.rpm, p.rpm_txt, sizeof(p.rpm_txt), buf);

    dash_gear_text(s, buf, sizeof(buf));
    dash_set_text_if(p.gear, p.gear_txt, sizeof(p.gear_txt), buf);
    int8_t gs = s->flashing ? (s->blink_fast ? 1 : 0) : 0;
    if (gs != p.gear_state) {
        lv_obj_set_style_bg_color(p.gear_box, gs ? C_BLUE : C_PANEL, 0);
        lv_obj_set_style_border_color(p.gear_box, gs ? C_BLUE : C_LINE, 0);
        p.gear_state = gs;
    }

    for (size_t i = 0; i < N_TL + N_TR; i++) dash_tile_update(&p.tiles[i], s);
    dash_flags_update(&p.flags, s);
}

const dash_page_t page_sweep = { "KAAR", 44, create, update, NULL, NULL, 938, 4, 16, 4, 856, 4 };
