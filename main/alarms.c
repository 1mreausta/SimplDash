#include "alarms.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "stats.h"
#include "store.h"

static const dash_warning_t WARN[] = DASH_WARNINGS;
#define N_WARN ((int)(sizeof(WARN) / sizeof(WARN[0])))
static const dash_ecu_alert_t ECU_AL[] = DASH_ECU_ALERTS;
#define N_ECU ((int)(sizeof(ECU_AL) / sizeof(ECU_AL[0])))

#define ACK_US       60000000LL   /* kinnitatud täisekraani hoiatus ei tule 60 s tagasi */
#define RELOG_US     30000000LL   /* sama hoiatust ei logita uuesti 30 s jooksul */
#define ON_DELAY_US  300000LL     /* piiri ületus peab kestma 0,3 s (müra / lühikesed piigid) */

static struct {
    alarm_event_t log[DASH_ALARM_LOG_N];
    int     n, head;              /* ringpuhver: head = järgmine kirjutatav */
    int     new_n;
    int64_t over_since[SET_MAX_WARN];
    int64_t last_log[SET_MAX_WARN + 8];
    int64_t ecu_until[8];
    int64_t ack_until[SET_MAX_WARN + 8];
    bool    was[SET_MAX_WARN + 8];
    uint16_t last_err;
} a;

typedef struct {
    uint16_t ver;
    uint16_t n, head;
    alarm_event_t log[DASH_ALARM_LOG_N];
} saved_t;

void alarms_init(void)
{
    static saved_t sv;
    if (store_load("alarmlog", &sv, sizeof(sv)) && sv.ver == 1 && sv.n <= DASH_ALARM_LOG_N &&
        sv.head < DASH_ALARM_LOG_N) {
        memcpy(a.log, sv.log, sizeof(a.log));
        a.n = sv.n;
        a.head = sv.head;
    }
}

static void save_log(void)
{
    static saved_t sv;
    sv.ver = 1;
    sv.n = (uint16_t)a.n;
    sv.head = (uint16_t)a.head;
    memcpy(sv.log, a.log, sizeof(a.log));
    store_save("alarmlog", &sv, sizeof(sv));
}

int alarms_log_count(void) { return a.n; }
int alarms_new_count(void) { return a.new_n; }

const alarm_event_t *alarms_log_get(int i)
{
    if (i < 0 || i >= a.n) return NULL;
    return &a.log[((a.head - 1 - i) % DASH_ALARM_LOG_N + DASH_ALARM_LOG_N) % DASH_ALARM_LOG_N];
}

void alarms_log_clear(void)
{
    a.n = a.head = a.new_n = 0;
    save_log();
}

const char *alarms_event_label(const alarm_event_t *e)
{
    if (e->kind == 0 && e->idx >= 0 && e->idx < N_WARN) return WARN[e->idx].label;
    if (e->kind == 1 && e->idx >= 0 && e->idx < N_ECU) return ECU_AL[e->idx].label;
    return "?";
}

static void add_event(const dash_state_t *s, int kind, int idx, float value, float limit)
{
    alarm_event_t *e = &a.log[a.head];
    memset(e, 0, sizeof(*e));
    e->uptime_s = (uint32_t)(s->now_us / 1000000);
    e->engine_s = stats_get()->engine_s;
    e->odo_m = (uint32_t)stats_get()->odo_m;
    if (s->gps.time_valid) {
        e->year = (int16_t)s->gps.year;
        e->mon = (int8_t)s->gps.month;
        e->day = (int8_t)s->gps.day;
        e->hour = (int8_t)s->gps.hour;
        e->min = (int8_t)s->gps.min;
        e->sec = (int8_t)s->gps.sec;
    }
    e->kind = (int8_t)kind;
    e->idx = (int8_t)idx;
    e->value = value;
    e->limit = limit;
    e->rpm = (int16_t)s->rpm;
    a.head = (a.head + 1) % DASH_ALARM_LOG_N;
    if (a.n < DASH_ALARM_LOG_N) a.n++;
    a.new_n++;
    save_log();
}

float alarms_oil_min(float rpm)
{
    const int n = DASH_OIL_CURVE_N;
    if (rpm <= g_set.oil_rpm[0]) return g_set.oil_kpa[0];
    for (int i = 1; i < n; i++) {
        if (rpm <= g_set.oil_rpm[i]) {
            float r0 = g_set.oil_rpm[i - 1], r1 = g_set.oil_rpm[i];
            float f = r1 > r0 ? (rpm - r0) / (r1 - r0) : 1;
            return g_set.oil_kpa[i - 1] + f * (g_set.oil_kpa[i] - g_set.oil_kpa[i - 1]);
        }
    }
    return g_set.oil_kpa[n - 1];
}

void alarms_ack(void)
{
    /* aeg pannakse järgmises eval-is (-1 = kinnitatud, aeg veel määramata) */
    for (int i = 0; i < SET_MAX_WARN + 8; i++) {
        if (a.was[i]) a.ack_until[i] = -1;
    }
}

void alarms_eval(dash_state_t *s)
{
    const int64_t now = s->now_us;
    char txt[96] = "";
    int nw = 0;
    s->alert_full = false;
    s->alert_txt[0] = 0;

    for (int i = 0; i < N_WARN; i++) {
        const dash_warning_t *w = &WARN[i];
        bool over = false;
        float v = s->d.v[w->sig], lim = NAN;
        float lo = g_set.warn_low[i], hi = g_set.warn_high[i], min_rpm = w->min_rpm;
        if (w->sig == SIG_OIL_PRESS && g_set.oil_curve_on && !isnan(lo)) {
            lo = alarms_oil_min(s->rpm);
            min_rpm = DASH_OIL_MIN_RPM;
        }
        if (!s->stale && s->d.valid[w->sig] && !(min_rpm > 0 && s->rpm < min_rpm)) {
            if (!isnan(hi) && v > hi) { over = true; lim = hi; }
            if (!isnan(lo) && v < lo) { over = true; lim = lo; }
        }
        /* lühike piik ei ole hoiatus */
        if (over) {
            if (!a.over_since[i]) a.over_since[i] = now;
            over = now - a.over_since[i] >= ON_DELAY_US;
        } else {
            a.over_since[i] = 0;
        }
        s->warn[i] = over;
        if (over) {
            size_t len = strlen(txt);
            snprintf(txt + len, sizeof(txt) - len, "%s%s", nw ? "  ·  " : "", w->label);
            nw++;
            if (!a.was[i] && (!a.last_log[i] || now - a.last_log[i] > RELOG_US)) {
                add_event(s, 0, i, v, lim);
                a.last_log[i] = now;
            }
            if (a.ack_until[i] == -1) a.ack_until[i] = now + ACK_US;
            if (w->full && g_set.alert_full && now >= a.ack_until[i] && !s->alert_full) {
                s->alert_full = true;
                snprintf(s->alert_txt, sizeof(s->alert_txt), "%s", w->label);
            }
        }
        a.was[i] = over;
    }

    /* ECU olekuhoiatused */
    for (int j = 0; j < N_ECU && j < 8; j++) {
        const dash_ecu_alert_t *e = &ECU_AL[j];
        const int k = SET_MAX_WARN + j;
        bool on = false;
        float val = 0;
        if (!s->stale) {
            if (e->flag) {
                on = (s->d.flags & e->flag) != 0;
                val = s->d.valid[SIG_KNOCK_COUNT] ? s->d.v[SIG_KNOCK_COUNT] : 0;
            } else if (s->d.valid[SIG_ERROR_CODE]) {
                on = s->d.v[SIG_ERROR_CODE] != 0;
                val = s->d.v[SIG_ERROR_CODE];
            }
        }
        if (on) a.ecu_until[j] = now + (int64_t)e->hold_ms * 1000;
        bool act = on || now < a.ecu_until[j];
        if (act) {
            size_t len = strlen(txt);
            if (e->flag == 0 && s->d.valid[SIG_ERROR_CODE])
                snprintf(txt + len, sizeof(txt) - len, "%s%s %d", nw ? "  ·  " : "", e->label,
                         (int)s->d.v[SIG_ERROR_CODE]);
            else
                snprintf(txt + len, sizeof(txt) - len, "%s%s", nw ? "  ·  " : "", e->label);
            nw++;
            if (!a.was[k] && (!a.last_log[k] || now - a.last_log[k] > RELOG_US)) {
                add_event(s, 1, j, val, NAN);
                a.last_log[k] = now;
            }
            if (a.ack_until[k] == -1) a.ack_until[k] = now + ACK_US;
            if (e->full && g_set.alert_full && now >= a.ack_until[k] && !s->alert_full) {
                s->alert_full = true;
                snprintf(s->alert_txt, sizeof(s->alert_txt), "%s", e->label);
            }
        }
        a.was[k] = act;
    }
    s->n_warn = nw;
    snprintf(s->warn_txt, sizeof(s->warn_txt), "%s", txt);
}
