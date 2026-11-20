#include "settings.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "store.h"

dash_settings_t g_set;

static const dash_warning_t WARN[] = DASH_WARNINGS;
#define N_WARN ((int)(sizeof(WARN) / sizeof(WARN[0])))
_Static_assert(sizeof(WARN) / sizeof(WARN[0]) <= SET_MAX_WARN, "liiga palju hoiatusi");

#define SHIFT_MIN 1000
#define SHIFT_MAX 12000

static int64_t s_dirty_us;
static bool s_dirty;

#define MAX_ITEMS 72
static set_item_t s_items[MAX_ITEMS];
static int s_n;
static const char *const *s_page_names;
static int s_n_pages;

static void defaults(dash_settings_t *c)
{
    memset(c, 0, sizeof(*c));
    c->version = SET_VERSION;
    c->shift_start = DASH_SHIFT_START;
    c->shift_flash = DASH_SHIFT_FLASH;
    for (int i = 0; i < SET_MAX_WARN; i++) c->warn_low[i] = c->warn_high[i] = NAN;
    for (int i = 0; i < N_WARN; i++) {
        c->warn_low[i] = WARN[i].low;
        c->warn_high[i] = WARN[i].high;
    }
    static const float OIL[DASH_OIL_CURVE_N][2] = DASH_OIL_CURVE;
    c->oil_curve_on = DASH_OIL_CURVE_ON;
    for (int i = 0; i < DASH_OIL_CURVE_N; i++) {
        c->oil_rpm[i] = (int16_t)OIL[i][0];
        c->oil_kpa[i] = OIL[i][1];
    }
    c->alert_full = 1;
    c->inj_cc = DASH_INJ_CC_MIN;
    c->cylinders = DASH_CYLINDERS;
    c->unit_temp_f = DASH_UNIT_TEMP_F;
    c->unit_press_psi = DASH_UNIT_PRESS_PSI;
    c->tz_offset_min = DASH_TZ_OFFSET_MIN;
    c->tz_dst = DASH_TZ_EU_DST;
    c->speed_src = DASH_SPEED_SOURCE;
    c->pages_off = 0;
    c->log_mode = DASH_LOG_MODE;
    c->log_hz = DASH_LOG_HZ;
    c->wifi_on = DASH_WIFI_ON;
    snprintf(c->wifi_ssid, sizeof(c->wifi_ssid), "%s", DASH_WIFI_SSID);
    snprintf(c->wifi_pass, sizeof(c->wifi_pass), "%s", DASH_WIFI_PASS);
    c->can_tx_on = DASH_CAN_TX_ON;
}

void settings_set_pages(const char *const *names, int n)
{
    s_page_names = names;
    s_n_pages = n;
}

/* --- loend ------------------------------------------------------------- */
static set_item_t *add(const char *key, const char *label, set_type_t t, void *ptr)
{
    if (s_n >= MAX_ITEMS) return NULL;
    set_item_t *it = &s_items[s_n++];
    memset(it, 0, sizeof(*it));
    snprintf(it->key, sizeof(it->key), "%s", key);
    snprintf(it->label, sizeof(it->label), "%s", label);
    it->type = t;
    it->ptr = ptr;
    it->unit = "";
    return it;
}

static void head(const char *label)
{
    char k[24];
    snprintf(k, sizeof(k), "h%d", s_n);
    add(k, label, ST_HEAD, NULL);
}

static set_item_t *ival(const char *key, const char *label, void *ptr, int size, float mn, float mx, float step,
                        const char *unit)
{
    set_item_t *it = add(key, label, ST_INT, ptr);
    it->size = (uint8_t)size;
    it->min = mn;
    it->max = mx;
    it->step = step;
    it->unit = unit;
    return it;
}

static set_item_t *fval(const char *key, const char *label, float *ptr, float mn, float mx, float step, int dec,
                        unit_kind_t k, const char *unit, bool nan_ok)
{
    set_item_t *it = add(key, label, ST_FLOAT, ptr);
    it->min = mn;
    it->max = mx;
    it->step = step;
    it->decimals = (uint8_t)dec;
    it->kind = k;
    it->unit = unit;
    it->nan_ok = nan_ok;
    return it;
}

static set_item_t *eval(const char *key, const char *label, int8_t *ptr, const char *const *opts, int n)
{
    set_item_t *it = add(key, label, ST_ENUM, ptr);
    it->opts = opts;
    it->max = (float)(n - 1);
    it->step = 1;
    return it;
}

static const char *const OFFON[] = { "VÄLJAS", "SEES" };
static const char *const TEMPU[] = { "°C", "°F" };
static const char *const PRESSU[] = { "bar", "psi" };
static const char *const SPEEDSRC[] = { "ECU", "GPS", "AUTO" };
static const char *const LOGM[] = { "VÄLJAS", "MOOTOR TÖÖTAB", "ALATI" };

static unit_kind_t kind_of(sig_id_t sig)
{
    switch (sig) {
    case SIG_CLT: case SIG_OIL_TEMP: case SIG_IAT: case SIG_EGT_MAX: case SIG_EGT1: case SIG_TRANS_TEMP:
    case SIG_DIFF_TEMP: case SIG_ECU_TEMP:
        return UK_TEMP;
    case SIG_OIL_PRESS: case SIG_FUEL_PRESS: case SIG_WG_PRESS: case SIG_CLT_PRESS: case SIG_MAP:
        return UK_KPA;
    default:
        return UK_NONE;
    }
}

static void build(void)
{
    s_n = 0;
    head("HOIATUSED");
    for (int i = 0; i < N_WARN; i++) {
        unit_kind_t k = kind_of(WARN[i].sig);
        const char *u = WARN[i].sig == SIG_BATT ? "V" : "";
        int dec = k == UK_KPA ? 1 : WARN[i].sig == SIG_BATT ? 1 : 0;
        float step = k == UK_KPA ? 10 : WARN[i].sig == SIG_BATT ? 0.1f : k == UK_TEMP ? 1 : 1;
        float mx = k == UK_KPA ? 1000 : k == UK_TEMP ? 1200 : 100;
        char key[24], lbl[40];
        if (!isnan(WARN[i].low)) {
            snprintf(key, sizeof(key), "w%d_lo", i);
            snprintf(lbl, sizeof(lbl), "%s min", WARN[i].label);
            fval(key, lbl, &g_set.warn_low[i], 0, mx, step, dec, k, u, true);
        }
        if (!isnan(WARN[i].high)) {
            snprintf(key, sizeof(key), "w%d_hi", i);
            snprintf(lbl, sizeof(lbl), "%s max", WARN[i].label);
            fval(key, lbl, &g_set.warn_high[i], 0, mx, step, dec, k, u, true);
        }
    }
    eval("alert_full", "Suur hoiatus üle ekraani", &g_set.alert_full, OFFON, 2);
    head("ÕLIRÕHK PÖÖRETE JÄRGI");
    eval("oil_on", "Õlirõhu kõver", &g_set.oil_curve_on, OFFON, 2);
    for (int i = 0; i < DASH_OIL_CURVE_N; i++) {
        char key[24], lbl[40];
        snprintf(key, sizeof(key), "oil%d_rpm", i);
        snprintf(lbl, sizeof(lbl), "Punkt %d: pöörded", i + 1);
        ival(key, lbl, &g_set.oil_rpm[i], 2, 0, 12000, 100, "rpm");
        snprintf(key, sizeof(key), "oil%d_min", i);
        snprintf(lbl, sizeof(lbl), "Punkt %d: min rõhk", i + 1);
        fval(key, lbl, &g_set.oil_kpa[i], 0, 1000, 10, 1, UK_KPA, "", false);
    }
    head("SHIFT-LIGHT");
    ival("shift_start", "Esimene tuli", &g_set.shift_start, 2, SHIFT_MIN, SHIFT_MAX - 100, 100, "rpm");
    ival("shift_flash", "Vaheta käiku (vilgub)", &g_set.shift_flash, 2, SHIFT_MIN + 100, SHIFT_MAX, 100, "rpm");
    head("MOOTOR");
    ival("inj_cc", "Pihusti vooluhulk", &g_set.inj_cc, 2, 50, 3000, 10, "cc/min");
    ival("cyl", "Silindreid", &g_set.cylinders, 1, 1, 16, 1, "");
    head("ÜHIKUD JA KELL");
    set_item_t *it = eval("unit_t", "Temperatuur", &g_set.unit_temp_f, TEMPU, 2);
    it->restart = true;
    it = eval("unit_p", "Rõhk", &g_set.unit_press_psi, PRESSU, 2);
    it->restart = true;
    eval("speed_src", "Kiiruse allikas", &g_set.speed_src, SPEEDSRC, 3);
    ival("tz", "Ajavöönd (UTC +)", &g_set.tz_offset_min, 2, -720, 840, 60, "min");
    eval("dst", "Suveaeg (EL)", &g_set.tz_dst, OFFON, 2);
    head("LOGIMINE (SD-KAART)");
    eval("log_mode", "Logi", &g_set.log_mode, LOGM, 3);
    ival("log_hz", "Logi sagedus", &g_set.log_hz, 1, 1, 50, 1, "Hz");
    head("WI-FI");
    eval("wifi_on", "Wi-Fi pääsupunkt", &g_set.wifi_on, OFFON, 2)->restart = true;
    it = add("wifi_ssid", "Võrgu nimi", ST_TEXT, g_set.wifi_ssid);
    it->size = sizeof(g_set.wifi_ssid) - 1;
    it->restart = true;
    it = add("wifi_pass", "Parool (min 8)", ST_TEXT, g_set.wifi_pass);
    it->size = sizeof(g_set.wifi_pass) - 1;
    it->restart = true;
    head("ECU JUHTIMINE");
    eval("can_tx", "Nupud saadavad ECU-le", &g_set.can_tx_on, OFFON, 2);
    if (s_page_names) {
        head("VAATED");
        for (int i = 0; i < s_n_pages && i < 16; i++) {
            if (!strcmp(s_page_names[i], "SEADED")) continue;   /* SEADED ei saa peita */
            char key[24];
            snprintf(key, sizeof(key), "page%d", i);
            it = add(key, s_page_names[i], ST_PAGE, NULL);
            it->bit = (uint8_t)i;
            it->max = 1;
            it->step = 1;
        }
    }
}

int settings_items(const set_item_t **out)
{
    if (!s_n || (s_page_names && s_items[s_n - 1].type != ST_PAGE)) build();
    *out = s_items;
    return s_n;
}

/* --- ühikud -------------------------------------------------------------- */
float settings_disp(unit_kind_t k, float si)
{
    if (k == UK_TEMP) return g_set.unit_temp_f ? si * 1.8f + 32 : si;
    if (k == UK_KPA) return g_set.unit_press_psi ? si * 0.1450377f : si * 0.01f;
    return si;
}

float settings_from_disp(unit_kind_t k, float v)
{
    if (k == UK_TEMP) return g_set.unit_temp_f ? (v - 32) / 1.8f : v;
    if (k == UK_KPA) return g_set.unit_press_psi ? v / 0.1450377f : v * 100;
    return v;
}

const char *settings_unit(unit_kind_t k)
{
    if (k == UK_TEMP) return g_set.unit_temp_f ? "°F" : "°C";
    if (k == UK_KPA) return g_set.unit_press_psi ? "psi" : "bar";
    return "";
}

/* --- väärtused -------------------------------------------------------- */
static int get_int(const set_item_t *it)
{
    if (it->type == ST_PAGE) return !((g_set.pages_off >> it->bit) & 1);
    return it->size == 2 ? *(int16_t *)it->ptr : *(int8_t *)it->ptr;
}

static void set_int(const set_item_t *it, int v)
{
    if (v < it->min) v = (int)it->min;
    if (v > it->max) v = (int)it->max;
    if (it->type == ST_PAGE) {
        if (v) g_set.pages_off &= (uint16_t)~(1u << it->bit);
        else g_set.pages_off |= (uint16_t)(1u << it->bit);
    } else if (it->size == 2) {
        *(int16_t *)it->ptr = (int16_t)v;
    } else {
        *(int8_t *)it->ptr = (int8_t)v;
    }
}

static int dec_for(const set_item_t *it)
{
    /* psi ja °F: vähem kohti, bar: 1-2 kohta */
    if (it->kind == UK_KPA) return g_set.unit_press_psi ? 0 : (it->step < 10 ? 2 : 1);
    if (it->kind == UK_TEMP) return 0;
    return it->decimals;
}

void settings_item_text(const set_item_t *it, char *buf, int cap)
{
    switch (it->type) {
    case ST_HEAD: buf[0] = 0; break;
    case ST_TEXT: snprintf(buf, cap, "%s", (const char *)it->ptr); break;
    case ST_PAGE: snprintf(buf, cap, "%s", get_int(it) ? "NÄHTAV" : "PEIDUS"); break;
    case ST_ENUM: snprintf(buf, cap, "%s", it->opts[get_int(it)]); break;
    case ST_INT: snprintf(buf, cap, "%d %s", get_int(it), it->unit); break;
    case ST_FLOAT: {
        float v = *(float *)it->ptr;
        if (isnan(v)) snprintf(buf, cap, "VÄLJAS");
        else snprintf(buf, cap, "%.*f %s", dec_for(it), settings_disp(it->kind, v),
                      it->kind ? settings_unit(it->kind) : it->unit);
        break;
    }
    }
}

void settings_item_value(const set_item_t *it, char *buf, int cap)
{
    if (it->type == ST_HEAD) buf[0] = 0;
    else if (it->type == ST_TEXT) snprintf(buf, cap, "%s", (const char *)it->ptr);
    else if (it->type != ST_FLOAT) snprintf(buf, cap, "%d", get_int(it));
    else if (isnan(*(float *)it->ptr)) buf[0] = 0;
    else snprintf(buf, cap, "%.*f", dec_for(it), settings_disp(it->kind, *(float *)it->ptr));
}

/* "Vaheta käiku" peab olema vähemalt 100 rpm üle "Esimene tuli" (muidu RACE vaade jagab nulliga);
 * muudetud väärtus jääb, teine nihkub kaasa */
static void fix_shift(const void *changed)
{
    if (g_set.shift_flash < g_set.shift_start + 100) {
        if (changed == &g_set.shift_flash) g_set.shift_start = (int16_t)(g_set.shift_flash - 100);
        else g_set.shift_flash = (int16_t)(g_set.shift_start + 100);
    }
    /* nihutatud väärtus ei tohi minna üle seade piiride */
    if (g_set.shift_flash > SHIFT_MAX) g_set.shift_flash = SHIFT_MAX;
    if (g_set.shift_flash < SHIFT_MIN + 100) g_set.shift_flash = SHIFT_MIN + 100;
    if (g_set.shift_start > g_set.shift_flash - 100) g_set.shift_start = (int16_t)(g_set.shift_flash - 100);
    if (g_set.shift_start < SHIFT_MIN) g_set.shift_start = SHIFT_MIN;
}

void settings_item_step(const set_item_t *it, int dir)
{
    switch (it->type) {
    case ST_HEAD: case ST_TEXT: return;
    case ST_ENUM: case ST_PAGE: {
        int n = (int)it->max + 1, v = get_int(it);
        set_int(it, ((v + dir) % n + n) % n);
        break;
    }
    case ST_INT: set_int(it, get_int(it) + dir * (int)it->step); break;
    case ST_FLOAT: {
        float *p = it->ptr;
        /* samm kuvatavates ühikutes: bar 0.1, psi 1, °F 1 */
        float step = it->kind == UK_KPA ? (g_set.unit_press_psi ? 1.0f : 0.1f) : it->step;
        if (isnan(*p)) {
            if (dir > 0) *p = it->min;
            break;
        }
        float d = settings_disp(it->kind, *p) + dir * step;
        float v = settings_from_disp(it->kind, d);
        if (v < it->min - 1e-3f) {
            *p = it->nan_ok ? NAN : it->min;
        } else {
            *p = v > it->max ? it->max : v;
        }
        break;
    }
    }
    fix_shift(it->ptr);
    settings_changed();
}

bool settings_item_set(const set_item_t *it, const char *txt)
{
    switch (it->type) {
    case ST_HEAD: return false;
    case ST_TEXT:
        if (strlen(txt) > it->size) return false;
        snprintf((char *)it->ptr, (size_t)it->size + 1, "%s", txt);
        break;
    case ST_ENUM: case ST_PAGE: {
        int n = (int)it->max + 1;
        for (int i = 0; i < n; i++) {
            if (it->type == ST_ENUM ? !strcmp(txt, it->opts[i]) : 0) { set_int(it, i); goto done; }
        }
        char *e;
        long v = strtol(txt, &e, 10);
        if (e == txt || v < 0 || v >= n) return false;
        set_int(it, (int)v);
        break;
    }
    case ST_INT: {
        char *e;
        long v = strtol(txt, &e, 10);
        if (e == txt) return false;
        set_int(it, (int)v);
        break;
    }
    case ST_FLOAT: {
        float *p = it->ptr;
        if (!*txt || !strcmp(txt, "VÄLJAS") || !strcmp(txt, "-")) {
            if (!it->nan_ok) return false;
            *p = NAN;
            break;
        }
        char *e;
        float v = strtof(txt, &e);
        if (e == txt) return false;
        v = settings_from_disp(it->kind, v);
        *p = v < it->min ? it->min : v > it->max ? it->max : v;
        break;
    }
    }
done:
    fix_shift(it->ptr);
    settings_changed();
    return true;
}

/* --- salvestus --------------------------------------------------------- */
void settings_init(void)
{
    defaults(&g_set);
    dash_settings_t c = g_set;   /* vanemas versioonis puuduvad väljad jäävad vaikeväärtusteks */
    if (store_load_grow("settings", &c, sizeof(c)) && c.version == SET_VERSION) g_set = c;
    g_set.wifi_ssid[sizeof(g_set.wifi_ssid) - 1] = 0;
    g_set.wifi_pass[sizeof(g_set.wifi_pass) - 1] = 0;
    fix_shift(NULL);
}

void settings_changed(void)
{
    s_dirty = true;
    s_dirty_us = 0;
}

void settings_tick(int64_t now_us)
{
    if (!s_dirty) return;
    if (!s_dirty_us) {
        s_dirty_us = now_us;
        return;
    }
    if (now_us - s_dirty_us > 2000000) {
        store_save("settings", &g_set, SET_DATA_END);
        s_dirty = false;
    }
}

void settings_reset(void)
{
    defaults(&g_set);
    settings_changed();
}
