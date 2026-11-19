/*
 * Vaade RACE: shift lights | segmenttahhomeeter | vasak tulp – käik/kiirus – parem tulp | alumine rida
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dash_common.h"

#define N_LEDS  15
#define N_SEGS  72

static const dash_tile_t TL[] = DASH_TILES_LEFT;
static const dash_tile_t TR[] = DASH_TILES_RIGHT;
static const dash_tile_t TB[] = DASH_TILES_BOTTOM;
#define N_TL (sizeof(TL) / sizeof(TL[0]))
#define N_TR (sizeof(TR) / sizeof(TR[0]))
#define N_TB (sizeof(TB) / sizeof(TB[0]))
#define N_TILES (N_TL + N_TR + N_TB)

static struct {
    lv_obj_t *leds[N_LEDS];
    int8_t    led_state[N_LEDS];       /* 0 väljas, 1 värv, 2 sinine */
    lv_obj_t *segs[N_SEGS];
    int8_t    seg_on[N_SEGS];
    lv_obj_t *scale_lbl[10];
    int       rev_limit;
    dash_tile_obj_t tiles[N_TILES];
    dash_flags_t flags;
    lv_obj_t *gear, *speed, *rpm;
    char      gear_txt[4], speed_txt[8], rpm_txt[8];
    bool      gear_blue;
} p;

static lv_color_t seg_color(int i)
{
    int rpm = (i + 1) * DASH_RPM_MAX / N_SEGS;
    if (rpm >= DASH_SHIFT_FLASH) return C_RED;
    if (rpm >= DASH_SHIFT_START) return C_YELLOW;
    return C_GREEN;
}

static void paint_segments_off(void)
{
    for (int i = 0; i < N_SEGS; i++) {
        int rpm = (i + 1) * DASH_RPM_MAX / N_SEGS;
        lv_obj_set_style_bg_color(p.segs[i], rpm > p.rev_limit ? C_LIMIT : C_OFF, 0);
        p.seg_on[i] = 0;
    }
    for (int k = 0; k < 10; k++) {
        if (p.scale_lbl[k]) {
            lv_obj_set_style_text_color(p.scale_lbl[k], k * 1000 >= p.rev_limit ? C_RED : C_DIM, 0);
        }
    }
}

static void create(lv_obj_t *scr)
{
    p.rev_limit = DASH_RPM_MAX - 500;

    /* Shift lights: y 8..28 */
    const int led_w = 46, led_gap = 8;
    int lx = (W - (N_LEDS * led_w + (N_LEDS - 1) * led_gap)) / 2;
    for (int i = 0; i < N_LEDS; i++) {
        p.leds[i] = dash_box(scr, lx + i * (led_w + led_gap), 8, led_w, 20, C_OFF, 10);
    }

    /* Tahhomeeter: segmendid y 38..104, skaala 106..128 */
    const int seg_w = 11, seg_gap = 3, seg_bot = 104;
    int sx = (W - (N_SEGS * seg_w + (N_SEGS - 1) * seg_gap)) / 2;
    for (int i = 0; i < N_SEGS; i++) {
        int h = 22 + (int)(44 * powf((float)i / (N_SEGS - 1), 1.4f));
        p.segs[i] = dash_box(scr, sx + i * (seg_w + seg_gap), seg_bot - h, seg_w, h, C_OFF, 2);
    }
    int span = N_SEGS * seg_w + (N_SEGS - 1) * seg_gap;
    for (int k = 0; k * 1000 <= DASH_RPM_MAX && k < 10; k++) {
        char t[4];
        snprintf(t, sizeof(t), "%d", k);
        p.scale_lbl[k] = dash_label(scr, 20, C_DIM, t);
        lv_obj_set_pos(p.scale_lbl[k], sx + k * 1000 * span / DASH_RPM_MAX - 4, 106);
    }
    paint_segments_off();

    /* Tulbad: y 136..496 */
    const int top = 136, tile_h = 84, gap = 8, col_w = 262;
    for (size_t i = 0; i < N_TL; i++) {
        dash_tile_create(&p.tiles[i], &TL[i], scr, 12, top + i * (tile_h + gap), col_w, tile_h, 52);
    }
    for (size_t i = 0; i < N_TR; i++) {
        dash_tile_create(&p.tiles[N_TL + i], &TR[i], scr, W - 12 - col_w, top + i * (tile_h + gap),
                         col_w, tile_h, 52);
    }
    /* Alumine rida: y 506..590 */
    const int bw = (W - 24 - (int)(N_TB - 1) * gap) / (int)N_TB;
    for (size_t i = 0; i < N_TB; i++) {
        dash_tile_create(&p.tiles[N_TL + N_TR + i], &TB[i], scr, 12 + i * (bw + gap), 506, bw, 84, 40);
    }

    /* Keskosa */
    const int cx = 12 + col_w + 12, cw = W - 2 * cx;
    lv_obj_t *center = dash_box(scr, cx, top, cw, 360, C_BG, 0);
    lv_obj_set_style_bg_opa(center, LV_OPA_TRANSP, 0);
    dash_flags_create(&p.flags, center, cw / 2, 0);

    p.gear = dash_label(center, 240, C_TEXT, "-");
    lv_obj_align(p.gear, LV_ALIGN_TOP_MID, 0, -8);
    p.speed = dash_value_with_unit(center, 72, C_TEXT, "km/h", -32, 10);
    p.rpm   = dash_value_with_unit(center, 28, C_DIM, "rpm", 0, 4);
}

static void update(const dash_state_t *s)
{
    float rpm = s->rpm;

    if (s->rev_limit != p.rev_limit) {
        p.rev_limit = s->rev_limit;
        paint_segments_off();
    }

    /* tahhomeeter */
    int lit = (int)(rpm / DASH_RPM_MAX * N_SEGS + 0.5f);
    for (int i = 0; i < N_SEGS; i++) {
        int8_t on = i < lit;
        if (on != p.seg_on[i]) {
            int seg_rpm = (i + 1) * DASH_RPM_MAX / N_SEGS;
            lv_obj_set_style_bg_color(p.segs[i], on ? (seg_rpm > p.rev_limit ? C_RED : seg_color(i))
                                                    : (seg_rpm > p.rev_limit ? C_LIMIT : C_OFF), 0);
            p.seg_on[i] = on;
        }
    }

    /* shift lights */
    int n = rpm <= DASH_SHIFT_START ? 0
            : (int)ceilf((rpm - DASH_SHIFT_START) / (DASH_SHIFT_FLASH - DASH_SHIFT_START) * N_LEDS);
    for (int i = 0; i < N_LEDS; i++) {
        int8_t st = s->flashing ? (s->blink_fast ? 2 : 0) : (i < n ? 1 : 0);
        if (st != p.led_state[i]) {
            lv_color_t c = st == 2 ? C_BLUE : st == 1 ? (i < 5 ? C_GREEN : i < 10 ? C_YELLOW : C_RED) : C_OFF;
            lv_obj_set_style_bg_color(p.leds[i], c, 0);
            p.led_state[i] = st;
        }
    }

    char buf[24];
    dash_gear_text(s, buf, sizeof(buf));
    dash_set_text_if(p.gear, p.gear_txt, sizeof(p.gear_txt), buf);
    if (s->flashing != p.gear_blue) {
        lv_obj_set_style_text_color(p.gear, s->flashing ? C_BLUE : C_TEXT, 0);
        p.gear_blue = s->flashing;
    }
    snprintf(buf, sizeof(buf), "%d", s->d.valid[SIG_SPEED] ? (int)(s->d.v[SIG_SPEED] + 0.5f) : 0);
    dash_set_text_if(p.speed, p.speed_txt, sizeof(p.speed_txt), buf);
    snprintf(buf, sizeof(buf), "%d", (int)rpm);
    dash_set_text_if(p.rpm, p.rpm_txt, sizeof(p.rpm_txt), buf);

    for (size_t i = 0; i < N_TILES; i++) dash_tile_update(&p.tiles[i], s);
    dash_flags_update(&p.flags, s);
}

const dash_page_t page_race = { "RACE", 132, create, update, NULL, NULL, 664, 137, 292, 134 };
