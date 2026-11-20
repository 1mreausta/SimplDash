#include "web_api.h"
#include <dirent.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "alarms.h"
#include "dash_ui.h"
#include "logger.h"
#include "settings.h"
#include "stats.h"
#include "version.h"

static char s_out[24 * 1024];
static size_t s_n;

static void out(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(s_out + s_n, sizeof(s_out) - s_n, fmt, ap);
    va_end(ap);
    if (n > 0) s_n = s_n + (size_t)n < sizeof(s_out) ? s_n + (size_t)n : sizeof(s_out) - 1;
}

/* JSON string */
static void js(const char *v)
{
    out("\"");
    for (const unsigned char *c = (const unsigned char *)v; *c; c++) {
        if (*c == '"' || *c == '\\') out("\\%c", *c);
        else if (*c < 0x20) out("\\u%04x", *c);
        else out("%c", *c);
    }
    out("\"");
}

static void jnum(float v, int dec)
{
    if (isnan(v) || isinf(v)) out("null");
    else out("%.*f", dec, v);
}

/* query parameeter (URL-dekodeeritud) */
static bool qparam(const char *uri, const char *key, char *val, size_t cap)
{
    const char *q = strchr(uri, '?');
    if (!q) return false;
    size_t kl = strlen(key);
    for (const char *p = q + 1; p && *p;) {
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            p += kl + 1;
            size_t n = 0;
            while (*p && *p != '&' && n + 1 < cap) {
                if (*p == '%' && p[1] && p[2]) {
                    char h[3] = { p[1], p[2], 0 };
                    val[n++] = (char)strtol(h, NULL, 16);
                    p += 3;
                } else {
                    val[n++] = *p == '+' ? ' ' : *p;
                    p++;
                }
            }
            val[n] = 0;
            return true;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return false;
}

static bool safe_name(const char *f)
{
    if (!*f || strlen(f) > 40) return false;
    for (const char *c = f; *c; c++) {
        if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_' ||
              *c == '.' || *c == '-'))
            return false;
    }
    return f[0] != '.';
}

static void status_json(void)
{
    const dash_state_t *s = dash_ui_state();
    const dash_stats_t *st = stats_get();
    logger_status_t ls;
    logger_status(&ls);
    char plat[160] = "";
    web_platform_status(plat, sizeof(plat));
    out("{\"version\":");
    js(SIMPLDASH_VERSION);
    out(",\"ecu\":%s,\"frames\":%u,\"rpm\":%.0f", s->stale ? "false" : "true", (unsigned)s->d.frame_count, s->rpm);
    out(",\"speed\":");
    jnum(s->d.valid[SIG_SPEED] ? s->d.v[SIG_SPEED] : NAN, 0);
    out(",\"clt\":");
    jnum(s->d.valid[SIG_CLT] ? settings_disp(UK_TEMP, s->d.v[SIG_CLT]) : NAN, 0);
    out(",\"oil\":");
    jnum(s->d.valid[SIG_OIL_PRESS] ? settings_disp(UK_KPA, s->d.v[SIG_OIL_PRESS]) : NAN, 1);
    out(",\"batt\":");
    jnum(s->d.valid[SIG_BATT] ? s->d.v[SIG_BATT] : NAN, 1);
    out(",\"temp_unit\":");
    js(settings_unit(UK_TEMP));
    out(",\"press_unit\":");
    js(settings_unit(UK_KPA));
    out(",\"warn\":");
    js(s->warn_txt);
    out(",\"gps\":{\"connected\":%s,\"fix\":%s,\"sats\":%d,\"hz\":%.1f,\"baud\":%d}", s->gps.connected ? "true" : "false",
        s->gps.fix ? "true" : "false", s->gps.sats, s->gps.rate_hz, s->gps.baud);
    out(",\"sd\":{\"card\":%s,\"active\":%s,\"file\":", ls.card ? "true" : "false", ls.active ? "true" : "false");
    js(ls.file);
    out(",\"rows\":%u,\"error\":%s}", (unsigned)ls.rows, ls.error ? "true" : "false");
    out(",\"odo_km\":%.1f,\"engine_h\":%.2f,\"trip_km\":%.2f,\"trip_l\":%.2f", st->odo_m / 1000, st->engine_s / 3600.0,
        st->trip_m / 1000, st->trip_fuel_l);
    float avg = stats_trip_avg();
    out(",\"trip_avg\":");
    jnum(avg < 0 ? NAN : avg, 1);
    out(",\"trip_s\":%u", (unsigned)st->trip_s);
    bool per_km;
    float now = stats_fuel_now(s, &per_km);
    out(",\"fuel_now\":");
    jnum(now < 0 ? NAN : now, 1);
    out(",\"fuel_now_unit\":\"%s\"", per_km ? "L/100km" : "L/h");
    out(",\"tank\":");
    jnum(s->d.valid[SIG_FUEL_TANK] ? s->d.v[SIG_FUEL_TANK] : NAN, 1);
    out(",\"platform\":");
    js(plat);
    out("}");
}

static void settings_json(void)
{
    const set_item_t *it;
    int n = settings_items(&it);
    out("[");
    for (int i = 0; i < n; i++) {
        const set_item_t *x = &it[i];
        static const char *T[] = { "int", "float", "enum", "text", "head", "page" };
        out("%s{\"key\":", i ? "," : "");
        js(x->key);
        out(",\"label\":");
        js(x->label);
        out(",\"type\":\"%s\"", T[x->type]);
        if (x->type == ST_HEAD) { out("}"); continue; }
        char v[80];
        settings_item_value(x, v, sizeof(v));
        out(",\"value\":");
        js(v);
        out(",\"unit\":");
        js(x->kind ? settings_unit(x->kind) : x->unit);
        if (x->type == ST_ENUM) {
            out(",\"opts\":[");
            for (int k = 0; k <= (int)x->max; k++) {
                out(k ? "," : "");
                js(x->opts[k]);
            }
            out("]");
        }
        if (x->type == ST_PAGE) out(",\"opts\":[\"PEIDUS\",\"NÄHTAV\"]");
        out(",\"nan_ok\":%s,\"restart\":%s}", x->nan_ok ? "true" : "false", x->restart ? "true" : "false");
    }
    out("]");
}

static void alarms_json(void)
{
    out("[");
    for (int i = 0; i < alarms_log_count(); i++) {
        const alarm_event_t *e = alarms_log_get(i);
        out("%s{\"label\":", i ? "," : "");
        js(alarms_event_label(e));
        if (e->year) out(",\"utc\":\"%04d-%02d-%02d %02d:%02d:%02d\"", e->year, e->mon, e->day, e->hour, e->min, e->sec);
        out(",\"engine_h\":%.2f,\"odo_km\":%.1f,\"rpm\":%d,\"value\":", e->engine_s / 3600.0, e->odo_m / 1000.0, e->rpm);
        jnum(e->value, 1);
        out(",\"limit\":");
        jnum(e->limit, 1);
        out("}");
    }
    out("]");
}

static void logs_json(void)
{
    const char *dir = logger_dir();
    out("[");
    if (dir) {
        DIR *d = opendir(dir);
        int k = 0;
        struct dirent *e;
        while (d && (e = readdir(d))) {
            if (!safe_name(e->d_name) || !strstr(e->d_name, ".csv")) continue;
            char p[256];
            struct stat st;
            snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
            if (stat(p, &st) != 0) continue;
            out("%s{\"name\":", k++ ? "," : "");
            js(e->d_name);
            out(",\"size\":%ld}", (long)st.st_size);
        }
        if (d) closedir(d);
    }
    out("]");
}

static void ok_json(bool ok, const char *msg)
{
    out("{\"ok\":%s,\"msg\":", ok ? "true" : "false");
    js(msg ? msg : "");
    out("}");
}

void web_api_handle(const char *uri, web_resp_t *r)
{
    s_n = 0;
    s_out[0] = 0;
    memset(r, 0, sizeof(*r));
    r->status = 200;
    r->type = "application/json";
    char path[96];
    size_t pl = strcspn(uri, "?");
    snprintf(path, sizeof(path), "%.*s", (int)(pl < sizeof(path) - 1 ? pl : sizeof(path) - 1), uri);

    if (!strcmp(path, "/") || !strcmp(path, "/web") || !strcmp(path, "/web/") || !strcmp(path, "/index.html")) {
        r->type = "text/html; charset=utf-8";
        r->body = web_page_html;
        r->len = strlen(web_page_html);
        return;
    }
    if (!strncmp(path, "/logs/", 6)) {
        const char *f = path + 6;
        const char *dir = logger_dir();
        if (!dir || !safe_name(f)) {
            r->status = 404;
        } else {
            snprintf(r->file, sizeof(r->file), "%s/%s", dir, f);
            snprintf(r->name, sizeof(r->name), "%s", f);
            r->type = "text/csv";
            return;
        }
    }
    char a[96], b[96];
    web_lock();
    if (!strcmp(path, "/api/status")) {
        status_json();
    } else if (!strcmp(path, "/api/settings")) {
        settings_json();
    } else if (!strcmp(path, "/api/alarms")) {
        alarms_json();
    } else if (!strcmp(path, "/api/logs")) {
        logs_json();
    } else if (!strcmp(path, "/api/set") && qparam(uri, "key", a, sizeof(a)) && qparam(uri, "val", b, sizeof(b))) {
        const set_item_t *it;
        int n = settings_items(&it);
        bool ok = false;
        for (int i = 0; i < n; i++) {
            if (!strcmp(it[i].key, a)) {
                if (!strcmp(a, "wifi_pass") && strlen(b) > 0 && strlen(b) < 8) break;
                if (!strcmp(a, "wifi_ssid") && !b[0]) break;   /* tühja nimega pääsupunkti ei leia */
                ok = settings_item_set(&it[i], b);
                break;
            }
        }
        ok_json(ok, ok ? "" : "vigane väärtus");
    } else if (!strcmp(path, "/api/trip_reset")) {
        stats_trip_reset();
        ok_json(true, NULL);
    } else if (!strcmp(path, "/api/odo") && qparam(uri, "km", a, sizeof(a))) {
        bool ok = stats_set_odo_km(atof(a));
        ok_json(ok, ok ? "" : "vigane läbisõit");
    } else if (!strcmp(path, "/api/alarms_clear")) {
        alarms_log_clear();
        ok_json(true, NULL);
    } else if (!strcmp(path, "/api/defaults")) {
        settings_reset();
        ok_json(true, NULL);
    } else if (!strcmp(path, "/api/del") && qparam(uri, "f", a, sizeof(a))) {
        const char *dir = logger_dir();
        logger_status_t ls;
        logger_status(&ls);
        bool ok = false;
        if (dir && safe_name(a) && !(ls.active && !strcmp(ls.file, a))) {
            snprintf(b, sizeof(b), "%s/%s", dir, a);
            ok = remove(b) == 0;
        }
        ok_json(ok, ok ? "" : "ei saa kustutada (logib praegu?)");
    } else {
        r->status = 404;
        ok_json(false, "pole sellist");
    }
    web_unlock();
    r->body = s_out;
    r->len = s_n;
}
