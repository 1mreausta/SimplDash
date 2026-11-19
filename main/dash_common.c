#include "dash_common.h"
#include <stdio.h>
#include <string.h>

/* Font (Barlow Condensed Bold, OFL) manustatud püsivarasse */
#ifdef DASH_FONT_C_ARRAY
/* veebisimulaator: font on C-massiivina (tools/web/build.sh genereerib) */
extern const uint8_t dash_font_ttf[];
extern const unsigned int dash_font_ttf_len;
#define font_ttf_start dash_font_ttf
#define font_ttf_end   (dash_font_ttf + dash_font_ttf_len)
#else
extern const uint8_t font_ttf_start[] asm("_binary_BarlowCondensed_Bold_ttf_start");
extern const uint8_t font_ttf_end[]   asm("_binary_BarlowCondensed_Bold_ttf_end");
#endif

static struct { int px; lv_font_t *f; } s_fonts[32];

lv_font_t *dash_font(int px)
{
    size_t i;
    for (i = 0; i < sizeof(s_fonts) / sizeof(s_fonts[0]) && s_fonts[i].f; i++) {
        if (s_fonts[i].px == px) return s_fonts[i].f;
    }
    if (i == sizeof(s_fonts) / sizeof(s_fonts[0])) {
        LV_LOG_WARN("liiga palju fondisuurusi (%d)", px);
        return s_fonts[0].f;
    }
    s_fonts[i].px = px;
    s_fonts[i].f = lv_tiny_ttf_create_data(font_ttf_start, font_ttf_end - font_ttf_start, px);
    return s_fonts[i].f;
}

lv_obj_t *dash_box(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t bg, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, bg, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_clickable(o, false);
    return o;
}

lv_obj_t *dash_label(lv_obj_t *parent, int px, lv_color_t color, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, dash_font(px), 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, txt);
    lv_obj_set_clickable(l, false);
    return l;
}

void dash_set_text_if(lv_obj_t *l, char *cache, size_t cap, const char *txt)
{
    if (strncmp(cache, txt, cap) != 0) {
        size_t n = strnlen(txt, cap - 1);  /* tahtlik kärpimine vahemälu suurusele */
        memcpy(cache, txt, n);
        cache[n] = 0;
        lv_label_set_text(l, txt);
    }
}

lv_obj_t *dash_value_with_unit(lv_obj_t *parent, int px, lv_color_t color, const char *unit,
                               int y_ofs, int unit_drop)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_set_scrollable(row, false);
    lv_obj_set_clickable(row, false);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, y_ofs);
    lv_obj_t *val = dash_label(row, px, color, "0");
    lv_obj_t *u = dash_label(row, 17, C_DIM, unit);
    lv_obj_set_style_pad_bottom(u, unit_drop, 0);
    return val;
}

lv_obj_t *dash_bar(lv_obj_t *parent, int w, int h, bool vertical)
{
    lv_obj_t *b = lv_bar_create(parent);
    lv_obj_set_size(b, w, h);
    lv_bar_set_range(b, 0, 1000);
    lv_obj_set_style_bg_color(b, C_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, C_ACCENT, LV_PART_INDICATOR);
    int r = (vertical ? w : h) / 2;
    lv_obj_set_style_radius(b, r, 0);
    lv_obj_set_style_radius(b, r, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(b, 0, 0);
    lv_obj_set_clickable(b, false);
    return b;
}

void dash_bar_set(lv_obj_t *bar, float v, float lo, float hi)
{
    float p = (v - lo) / (hi - lo);
    p = p < 0 ? 0 : p > 1 ? 1 : p;
    int32_t pv = (int32_t)(p * 1000);
    if (lv_bar_get_value(bar) != pv) lv_bar_set_value(bar, pv, LV_ANIM_OFF);
}

void dash_gear_text(const dash_state_t *s, char *buf, size_t cap)
{
    if (!s->d.valid[SIG_GEAR]) { snprintf(buf, cap, "-"); return; }
    int g = (int)s->d.v[SIG_GEAR];
    if (g == 0) snprintf(buf, cap, "N");
    else if (g < 0) snprintf(buf, cap, "R");
    else snprintf(buf, cap, "%d", g);
}

static const dash_warning_t WARN[] = DASH_WARNINGS;

bool dash_sig_alarm(const dash_state_t *s, sig_id_t sig)
{
    for (size_t i = 0; i < sizeof(WARN) / sizeof(WARN[0]) && i < DASH_MAX_WARN; i++) {
        if (WARN[i].sig == sig && s->warn[i]) return true;
    }
    return false;
}

dash_tile_t dash_units(dash_tile_t c)
{
    if (g_set.unit_temp_f && c.unit && !strcmp(c.unit, "°C")) {
        c.unit = "°F";
        c.scale *= 1.8f;
        c.offset = c.offset * 1.8f + 32;
        c.bar_min = c.bar_min * 1.8f + 32;
        c.bar_max = c.bar_max * 1.8f + 32;
    } else if (g_set.unit_press_psi && c.unit && !strcmp(c.unit, "bar")) {
        const float k = 14.50377f;
        c.unit = "psi";
        c.scale *= k;
        c.offset *= k;
        c.bar_min *= k;
        c.bar_max *= k;
        c.decimals = c.decimals > 1 ? 1 : 0;
    }
    return c;
}

float dash_tile_value(const dash_state_t *s, const dash_tile_t *cfg)
{
    return s->d.v[cfg->sig] * cfg->scale + cfg->offset;
}

void dash_fmt(const dash_state_t *s, const dash_tile_t *cfg, char *buf, size_t cap)
{
    if (s->d.valid[cfg->sig]) snprintf(buf, cap, "%.*f", cfg->decimals, dash_tile_value(s, cfg));
    else snprintf(buf, cap, "--");
}

/* ---------------------------------------------------------------------- */
void dash_tile_create(dash_tile_obj_t *t, const dash_tile_t *cfg, lv_obj_t *parent,
                      int x, int y, int w, int h, int val_px)
{
    memset(t, 0, sizeof(*t));
    t->cfg = dash_units(*cfg);
    cfg = &t->cfg;
    t->panel = dash_box(parent, x, y, w, h, C_PANEL, 8);
    lv_obj_set_style_border_color(t->panel, C_LINE, 0);
    lv_obj_set_style_border_width(t->panel, 1, 0);

    t->lbl = dash_label(t->panel, 17, C_DIM, cfg->label);
    lv_obj_align(t->lbl, LV_ALIGN_TOP_LEFT, 10, 5);
    t->unit = dash_label(t->panel, 17, C_DIM, cfg->unit);
    lv_obj_align(t->unit, LV_ALIGN_TOP_RIGHT, -10, 5);

    bool has_bar = cfg->bar_max > cfg->bar_min;
    t->val = dash_label(t->panel, val_px, C_TEXT, "--");
    lv_obj_align(t->val, LV_ALIGN_RIGHT_MID, -10, has_bar ? 2 : 10);

    if (has_bar) {
        t->bar = dash_bar(t->panel, w - 20, 5, false);
        lv_obj_align(t->bar, LV_ALIGN_BOTTOM_MID, 0, -7);
    }
    t->alarm = -1;
    t->cold = -1;
}

void dash_tile_update(dash_tile_obj_t *t, const dash_state_t *s)
{
    char buf[16];
    bool valid = s->d.valid[t->cfg.sig];
    float v = dash_tile_value(s, &t->cfg);
    dash_fmt(s, &t->cfg, buf, sizeof(buf));
    dash_set_text_if(t->val, t->last, sizeof(t->last), buf);
    if (t->bar && valid) dash_bar_set(t->bar, v, t->cfg.bar_min, t->cfg.bar_max);

    int8_t al = dash_sig_alarm(s, t->cfg.sig) && s->blink;
    if (al != t->alarm) {
        lv_obj_set_style_bg_color(t->panel, al ? C_RED : C_PANEL, 0);
        lv_obj_set_style_border_color(t->panel, al ? C_RED : C_LINE, 0);
        lv_obj_set_style_text_color(t->lbl, al ? C_TEXT : C_DIM, 0);
        lv_obj_set_style_text_color(t->unit, al ? C_TEXT : C_DIM, 0);
        t->alarm = al;
        t->cold = -1; /* värvi uuesti */
    }
    int8_t cold = t->cfg.sig == SIG_CLT && valid && s->d.v[SIG_CLT] < 60 && !al;
    if (cold != t->cold) {
        lv_obj_set_style_text_color(t->val, cold ? C_ACCENT : C_TEXT, 0);
        t->cold = cold;
    }
}

/* ---------------------------------------------------------------------- */
static const uint16_t FLAG_BITS[DASH_N_FLAGS] = { FLAG_LAUNCH, FLAG_ANTILAG, FLAG_REVLIMIT,
                                                  FLAG_KNOCK, FLAG_LOGGING };
static const char *FLAG_TXT[DASH_N_FLAGS] = { "LAUNCH", "ALS", "LIMIT", "KNOCK", "LOG" };
static const uint32_t FLAG_COL[DASH_N_FLAGS] = { 0xFFD21F, 0xFFD21F, 0xFF2D2D, 0xFF2D2D, 0x22E06B };
static const int FLAG_W[DASH_N_FLAGS] = { 64, 44, 54, 62, 44 };

void dash_flags_create(dash_flags_t *f, lv_obj_t *parent, int cx, int y)
{
    int total = 0;
    for (int i = 0; i < DASH_N_FLAGS; i++) total += FLAG_W[i] + (i ? 6 : 0);
    int x = cx - total / 2;
    for (int i = 0; i < DASH_N_FLAGS; i++) {
        lv_obj_t *b = dash_box(parent, x, y, FLAG_W[i], 26, lv_color_hex(0x11151A), 5);
        lv_obj_t *t = dash_label(b, 16, lv_color_hex(0x2B3139), FLAG_TXT[i]);
        lv_obj_center(t);
        f->box[i] = b;
        f->on[i] = 0;
        x += FLAG_W[i] + 6;
    }
}

void dash_flags_update(dash_flags_t *f, const dash_state_t *s)
{
    for (int i = 0; i < DASH_N_FLAGS; i++) {
        int8_t on = (s->d.flags & FLAG_BITS[i]) != 0 && !s->stale;
        if (on != f->on[i]) {
            lv_obj_set_style_bg_color(f->box[i], on ? lv_color_hex(FLAG_COL[i]) : lv_color_hex(0x11151A), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(f->box[i], 0),
                                        on ? (FLAG_COL[i] == 0xFF2D2D ? C_TEXT : C_BG)
                                           : lv_color_hex(0x2B3139), 0);
            f->on[i] = on;
        }
    }
}

/* ---------------------------------------------------------------------- */
bool dash_need_gps(const dash_state_t *s)
{
    return s->gps_ok;
}

void dash_fmt_time(float sec, bool minutes, char *buf, size_t cap)
{
    if (sec < 0) sec = 0;
    int cs = (int)(sec * 100 + 0.5f);
    if (minutes) snprintf(buf, cap, "%d:%02d.%02d", cs / 6000, (cs / 100) % 60, cs % 100);
    else snprintf(buf, cap, "%d.%02d", cs / 100, cs % 100);
}

lv_obj_t *dash_button(lv_obj_t *parent, const char *txt, int x, int y, int w, int h, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, C_PANEL, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, C_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, C_LINE, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_t *l = dash_label(btn, 22, C_TEXT, txt);
    lv_obj_center(l);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}
