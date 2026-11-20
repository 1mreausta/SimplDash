/*
 * Käitusajal muudetavad seaded (SEADED vaade ja Wi-Fi leht). Vaikeväärtused tulevad
 * dash_config.h-st; muudetud väärtused salvestatakse püsimällu (store).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "dash_config.h"

#define SET_MAX_WARN 16
#define SET_VERSION  1   /* muuda ainult siis, kui vana väli muutub; uued väljad lisa lõppu */

typedef struct {
    uint16_t version;
    /* shift-light */
    int16_t  shift_start, shift_flash;
    /* hoiatuste piirid (DASH_WARNINGS järjekorras; NAN = ei kontrolli) */
    float    warn_low[SET_MAX_WARN], warn_high[SET_MAX_WARN];
    /* õlirõhk pöörete järgi */
    int8_t   oil_curve_on;
    int16_t  oil_rpm[DASH_OIL_CURVE_N];
    float    oil_kpa[DASH_OIL_CURVE_N];
    int8_t   alert_full;        /* täisekraani hoiatused lubatud */
    /* kütusekulu */
    int16_t  inj_cc;
    int8_t   cylinders;
    /* ühikud ja kell */
    int8_t   unit_temp_f, unit_press_psi;
    int16_t  tz_offset_min;
    int8_t   tz_dst;
    int8_t   speed_src;
    /* vaated: bitt = vaade peidetud */
    uint16_t pages_off;
    /* logimine SD-kaardile: 0 väljas, 1 kui mootor töötab, 2 alati (ECU andmed tulevad) */
    int8_t   log_mode;
    int8_t   log_hz;
    /* Wi-Fi pääsupunkt */
    int8_t   wifi_on;
    char     wifi_ssid[33];
    char     wifi_pass[65];
    /* CAN: dash saadab ECU-le nuppude kaadri */
    int8_t   can_tx_on;
    /* UUED VÄLJAD SIIA (lõppu) ja uuenda SET_DATA_END */
} dash_settings_t;
/* salvestatakse ilma lõpu täitebaitideta, et järgmise versiooni uus väli ei satuks vanade täitebaitide kohale */
#define SET_DATA_END (offsetof(dash_settings_t, can_tx_on) + sizeof(int8_t))

extern dash_settings_t g_set;

/* Lae püsimälust (või vaikeväärtused). Kutsu käivitusel. */
void settings_init(void);
/* Märgi muudetuks – salvestatakse settings_tick-is ~2 s pärast viimast muutust */
void settings_changed(void);
/* Kutsu perioodiliselt (UI taimer) */
void settings_tick(int64_t now_us);
/* Taasta vaikeväärtused */
void settings_reset(void);

/* --- muudetavate seadete loend (ekraan + veebileht kasutavad sama) --- */
typedef enum { ST_INT, ST_FLOAT, ST_ENUM, ST_TEXT, ST_HEAD, ST_PAGE } set_type_t;
/* kuvamise ühik: väärtus hoitakse alati SI-s (°C, kPa), kuvatakse valitud ühikutes */
typedef enum { UK_NONE, UK_TEMP, UK_KPA } unit_kind_t;

typedef struct {
    char        key[24];       /* veebi jaoks */
    char        label[40];
    set_type_t  type;
    void       *ptr;           /* int16_t / int8_t (ENUM) / float / char[] */
    uint8_t     size;          /* täisarvu baitides (1 või 2) või teksti max pikkus */
    float       min, max, step;
    unit_kind_t kind;          /* UK_NONE: kuvatakse nagu on, ühik = unit */
    uint8_t     decimals;
    const char *unit;
    uint8_t     bit;           /* ST_PAGE: vaate number */
    const char *const *opts;   /* ENUM valikud */
    bool        nan_ok;        /* FLOAT: võib olla "väljas" (NAN) */
    bool        restart;       /* rakendub pärast taaskäivitust */
} set_item_t;

/* Vaadete nimed (SEADED → vaadete peitmine). Kutsu enne settings_items-i. */
void settings_set_pages(const char *const *names, int n);
/* Ühikute teisendus kuvamiseks (praeguste seadete järgi) */
float settings_disp(unit_kind_t k, float si);
float settings_from_disp(unit_kind_t k, float v);
const char *settings_unit(unit_kind_t k);
/* Loend (koostatakse settings_init-is). */
int settings_items(const set_item_t **out);
/* Väärtus kuvatava tekstina */
void settings_item_text(const set_item_t *it, char *buf, int cap);
/* Väärtus ilma ühikuta (veebileht): arv kuvatavates ühikutes, ENUM/PAGE indeks, FLOAT väljas = "" */
void settings_item_value(const set_item_t *it, char *buf, int cap);
/* Muuda sammu võrra (dir = +1 / -1); FLOAT nan_ok puhul min-st alla = väljas */
void settings_item_step(const set_item_t *it, int dir);
/* Määra väärtus tekstist (veebileht; kuvatavates ühikutes). true = õnnestus */
bool settings_item_set(const set_item_t *it, const char *txt);
