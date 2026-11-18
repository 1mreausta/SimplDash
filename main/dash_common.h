/*
 * Ühine osa kõigile vaadetele: värvid, fondid, abifunktsioonid, näidiku-plaat
 * ja iga kaadri olek (dash_state_t).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"
#include "dash_config.h"
#include "maxxecu.h"

#define W 1024
#define H 600

/* Värvid */
#define C_BG      lv_color_hex(0x000000)
#define C_PANEL   lv_color_hex(0x0B0D10)
#define C_LINE    lv_color_hex(0x1C2128)
#define C_TEXT    lv_color_hex(0xF2F4F7)
#define C_DIM     lv_color_hex(0x7D8794)
#define C_OFF     lv_color_hex(0x15181D)
#define C_TRACK   lv_color_hex(0x1A1E24)
#define C_GREEN   lv_color_hex(0x22E06B)
#define C_YELLOW  lv_color_hex(0xFFD21F)
#define C_RED     lv_color_hex(0xFF2D2D)
#define C_BLUE    lv_color_hex(0x2F7BFF)
#define C_ACCENT  lv_color_hex(0x00D4FF)
#define C_LIMIT   lv_color_hex(0x3A0B0B)

#define DASH_MAX_WARN 16

/* Ühe UI-kaadri olek – arvutatakse kord ja antakse aktiivsele vaatele */
typedef struct {
    ecu_data_t d;
    bool  stale;        /* ECU vaikib */
    bool  blink;        /* 2 Hz */
    bool  blink_fast;   /* shift flash */
    bool  flashing;     /* rpm >= DASH_SHIFT_FLASH */
    float rpm;
    int   rev_limit;
    bool  warn[DASH_MAX_WARN];
} dash_state_t;

/* Vaade */
typedef struct {
    const char *name;
    int  warn_y;                                  /* hoiatusriba ülemine serv */
    void (*create)(lv_obj_t *parent);
    void (*update)(const dash_state_t *s);        /* ainult kui vaade on nähtav */
    void (*background)(const dash_state_t *s);    /* alati (võib olla NULL) */
} dash_page_t;

extern const dash_page_t page_race, page_gauges, page_sweep, page_street, page_data;

/* Font Barlow Condensed Bold soovitud suuruses (loob esimesel korral) */
lv_font_t *dash_font(int px);

lv_obj_t *dash_box(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t bg, int radius);
lv_obj_t *dash_label(lv_obj_t *parent, int px, lv_color_t color, const char *txt);
void dash_set_text_if(lv_obj_t *l, char *cache, size_t cap, const char *txt);
/* suur number + väike ühik flex-reas (vanema alumise serva keskel, nihkega y_ofs) */
lv_obj_t *dash_value_with_unit(lv_obj_t *parent, int px, lv_color_t color, const char *unit,
                               int y_ofs, int unit_drop);
lv_obj_t *dash_bar(lv_obj_t *parent, int w, int h, bool vertical);
void dash_bar_set(lv_obj_t *bar, float v, float lo, float hi);

/* Käik tekstina: "-", "N", "R", "1".. */
void dash_gear_text(const dash_state_t *s, char *buf, size_t cap);
/* Kas mõni aktiivne hoiatus käib selle signaali kohta */
bool dash_sig_alarm(const dash_state_t *s, sig_id_t sig);
/* Väärtus tekstina ("--" kui pole) */
void dash_fmt(const dash_state_t *s, const dash_tile_t *cfg, char *buf, size_t cap);

/* Näidiku-plaat: silt, ühik, väärtus, riba. Värvub hoiatuse korral punaseks. */
typedef struct {
    dash_tile_t cfg;
    lv_obj_t   *panel, *val, *bar, *lbl, *unit;
    char        last[16];
    int8_t      alarm;
    int8_t      cold;
} dash_tile_obj_t;

void dash_tile_create(dash_tile_obj_t *t, const dash_tile_t *cfg, lv_obj_t *parent,
                      int x, int y, int w, int h, int val_px);
void dash_tile_update(dash_tile_obj_t *t, const dash_state_t *s);

/* Olekulippude rida (LAUNCH, ALS, LIMIT, KNOCK, LOG) */
#define DASH_N_FLAGS 5
typedef struct {
    lv_obj_t *box[DASH_N_FLAGS];
    int8_t    on[DASH_N_FLAGS];
} dash_flags_t;
/* Loob rea, mille keskpunkt on (cx, y) */
void dash_flags_create(dash_flags_t *f, lv_obj_t *parent, int cx, int y);
void dash_flags_update(dash_flags_t *f, const dash_state_t *s);
