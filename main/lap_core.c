#include "lap_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gps_pages.h"
#include "logger.h"
#include "store.h"
#ifndef DASH_HOST
#include "esp_heap_caps.h"
#endif

#define DEG2RAD 0.017453292519943295
#define AUTO_TRACK_M  3000.0    /* salvestatud rada aktiveerub, kui oled sellest lähemal */
#define DELTA_MAX_M   40.0      /* kaugemal parima ringi joonest delta ei kehti */

static lap_info_t L;
static struct {
    double   lat0, lon0, mlat, mlon;
    uint32_t last_sample;
    double   px, py;
    int64_t  pt;
    bool     have_prev;
    float    moving_course;
    float    cur_d;
    int      ref_idx;
    lap_pt_t *cur, *ref;
    bool     saved_checked;
} p;

typedef struct {
    uint16_t ver;
    double   lat0, lon0;
    float    course;
    lap_gate_t gate[LAP_N_SECT - 1];
    uint8_t  manual;
} saved_track_t;

const lap_info_t *lap_info(void) { return &L; }

static lap_pt_t *alloc_trace(void)
{
#ifndef DASH_HOST
    lap_pt_t *b = heap_caps_malloc(sizeof(lap_pt_t) * LAP_TRACE_MAX, MALLOC_CAP_SPIRAM);
    if (b) return b;
#endif
    return malloc(sizeof(lap_pt_t) * LAP_TRACE_MAX);
}

static void ensure_buffers(void)
{
    if (!p.cur) p.cur = alloc_trace();
    if (!p.ref) p.ref = alloc_trace();
    L.cur = p.cur;
    L.ref = p.ref;
}

static void to_xy(double lat, double lon, double *x, double *y)
{
    *x = (lon - p.lon0) * p.mlon;
    *y = (lat - p.lat0) * p.mlat;
}

/* ületas värava (eelmine punkt taga, praegune ees; külgsuunas ≤ DASH_LAP_GATE_M). f = osa lõigust */
static bool crossed(const lap_gate_t *g, double ax, double ay, double bx, double by, double *f)
{
    double sa = (ax - g->cx) * g->dx + (ay - g->cy) * g->dy;
    double sb = (bx - g->cx) * g->dx + (by - g->cy) * g->dy;
    if (!(sa < 0 && sb >= 0)) return false;
    *f = sa != sb ? -sa / (sb - sa) : 1.0;
    double cx = ax + *f * (bx - ax) - g->cx, cy = ay + *f * (by - ay) - g->cy;
    return fabs(cx * g->dy - cy * g->dx) <= DASH_LAP_GATE_M;
}

static void save_track(float course)
{
    saved_track_t t = { .ver = 2, .lat0 = p.lat0, .lon0 = p.lon0, .course = course, .manual = L.gates_manual };
    memcpy(t.gate, L.gate, sizeof(t.gate));
    store_save("track", &t, sizeof(t));
}

static void set_line(double lat, double lon, float course_deg)
{
    double c = course_deg * DEG2RAD;
    p.lat0 = lat;
    p.lon0 = lon;
    p.mlat = 110540.0;
    p.mlon = 111320.0 * cos(lat * DEG2RAD);
    L.line = (lap_gate_t){ 0, 0, sin(c), cos(c), true };
    L.line_set = true;
}

void lap_set_line(double lat, double lon, float course_deg, bool start_now)
{
    gps_data_t g;
    gps_snapshot(&g);
    ensure_buffers();
    set_line(lat, lon, course_deg);
    memset(L.gate, 0, sizeof(L.gate));
    L.gates_manual = false;
    L.n_ref = 0;
    L.n_cur = 0;
    L.running = start_now;
    L.laps = start_now ? L.done + 1 : L.done;
    L.lap_start = L.sec_start = g.sample_us;
    memset(L.sec_cur, 0, sizeof(L.sec_cur));
    L.sect = 0;
    p.cur_d = 0;
    p.ref_idx = 0;
    p.have_prev = false;
    p.last_sample = g.samples;
    p.saved_checked = true;
    save_track(course_deg);
}

void lap_seed_history(const float *laps, int n)
{
    if (L.n_hist || n <= 0) return;
    if (n > LAP_N_HIST) n = LAP_N_HIST;
    for (int i = 0; i < n; i++) {
        L.hist[i] = laps[i];
        if (L.best <= 0 || laps[i] < L.best) L.best = laps[i];
    }
    L.n_hist = n;
    L.done = n;
    L.laps = L.running ? n + 1 : n;
    L.last = laps[0];
    L.prev_best = 0;
    for (int i = 1; i < n; i++) if (L.prev_best <= 0 || laps[i] < L.prev_best) L.prev_best = laps[i];
}

void lap_mark_line(void)
{
    gps_data_t g;
    gps_snapshot(&g);
    if (!g.fix) return;
    lap_set_line(g.lat, g.lon, g.speed_kmh > 5 ? g.course : p.moving_course, true);
}

void lap_reset_times(void)
{
    L.running = false;
    L.last = L.best = L.prev_best = 0;
    L.n_hist = L.laps = L.done = 0;
    L.n_ref = L.n_cur = 0;
    L.delta_ok = false;
    memset(L.sec_cur, 0, sizeof(L.sec_cur));
    memset(L.sec_last, 0, sizeof(L.sec_last));
    memset(L.sec_best, 0, sizeof(L.sec_best));
    L.laps = 0;
}

/* värav raja punktis i (suund naabrite järgi) */
static bool gate_at(int k, int i)
{
    if (i < 1) i = 1;
    if (i > L.n_ref - 2) i = L.n_ref - 2;
    double dx = L.ref[i + 1].x - L.ref[i - 1].x, dy = L.ref[i + 1].y - L.ref[i - 1].y;
    double n = sqrt(dx * dx + dy * dy);
    if (n < 0.1) return false;
    L.gate[k] = (lap_gate_t){ L.ref[i].x, L.ref[i].y, dx / n, dy / n, true };
    return true;
}

static void reset_sectors(void)
{
    memset(L.sec_cur, 0, sizeof(L.sec_cur));
    memset(L.sec_last, 0, sizeof(L.sec_last));
    memset(L.sec_best, 0, sizeof(L.sec_best));
    L.sect = 0;
    L.sec_start = L.lap_start;
}

/* sektorite väravad parima ringi 1/3 ja 2/3 teekonna kohale */
static void make_gates(void)
{
    if (L.n_ref < 20) return;
    float len = L.ref[L.n_ref - 1].d;
    for (int k = 0; k < LAP_N_SECT - 1; k++) {
        float want = len * (k + 1) / LAP_N_SECT;
        int i = 1;
        while (i < L.n_ref - 2 && L.ref[i].d < want) i++;
        if (!gate_at(k, i)) return;
    }
    L.gates_manual = false;
}

/* lähim raja punkt (indeks) või -1 */
static int nearest_ref(float x, float y, float max_m)
{
    int best = -1;
    double bd = (double)max_m * max_m;
    for (int i = 0; i < L.n_ref; i++) {
        double dx = L.ref[i].x - x, dy = L.ref[i].y - y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void save_track(float course);

bool lap_set_gate(int k, float x, float y)
{
    if (k < 0 || k >= LAP_N_SECT - 1 || L.n_ref < 20) return false;
    int i = nearest_ref(x, y, 60);
    if (i < 0) return false;
    /* ära lase piiril olla stardijoonele liiga lähedal (ring algab/lõpeb seal) */
    if (L.ref[i].d < 20 || L.ref[i].d > L.ref[L.n_ref - 1].d - 20) return false;
    if (!gate_at(k, i)) return false;
    /* järjekord rajal: S1 lõpp enne S2 lõppu – vajadusel vaheta */
    if (L.gate[0].set && L.gate[1].set) {
        int i0 = nearest_ref((float)L.gate[0].cx, (float)L.gate[0].cy, 1e6f);
        int i1 = nearest_ref((float)L.gate[1].cx, (float)L.gate[1].cy, 1e6f);
        if (i0 > i1) {
            lap_gate_t t = L.gate[0];
            L.gate[0] = L.gate[1];
            L.gate[1] = t;
        }
    }
    L.gates_manual = true;
    reset_sectors();
    save_track(atan2(L.line.dx, L.line.dy) / DEG2RAD);
    return true;
}

void lap_auto_gates(void)
{
    make_gates();
    reset_sectors();
    save_track(atan2(L.line.dx, L.line.dy) / DEG2RAD);
}

/* delta: lähim punkt parimal ringil (otsitakse eelmise ümbert edasi) */
static void update_delta(double x, double y, float t)
{
    L.delta_ok = false;
    if (L.n_ref < 10) return;
    int best = -1;
    double bd = 1e18;
    int from = p.ref_idx - 5 < 0 ? 0 : p.ref_idx - 5;
    int to = p.ref_idx + 80 > L.n_ref - 1 ? L.n_ref - 1 : p.ref_idx + 80;
    for (int i = from; i < to; i++) {
        double dx = L.ref[i].x - x, dy = L.ref[i].y - y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    if (best < 0 || bd > DELTA_MAX_M * DELTA_MAX_M) return;
    p.ref_idx = best;
    /* projektsioon lõigule best → best+1 */
    const lap_pt_t *a = &L.ref[best], *b = &L.ref[best + 1 < L.n_ref ? best + 1 : best];
    double sx = b->x - a->x, sy = b->y - a->y, s2 = sx * sx + sy * sy;
    double f = s2 > 0 ? ((x - a->x) * sx + (y - a->y) * sy) / s2 : 0;
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    float tref = a->t + (float)f * (b->t - a->t);
    L.delta = t - tref;
    L.delta_ok = true;
}

static void lap_done(int64_t tc)
{
    float lap = (tc - L.lap_start) / 1e6f;
    /* viimane sektor */
    if (L.gate[0].set && L.sect == LAP_N_SECT - 1) L.sec_cur[LAP_N_SECT - 1] = (tc - L.sec_start) / 1e6f;
    bool full_sect = true;
    for (int k = 0; k < LAP_N_SECT; k++) full_sect &= L.sec_cur[k] > 0;
    if (full_sect) {
        memcpy(L.sec_last, L.sec_cur, sizeof(L.sec_last));
        for (int k = 0; k < LAP_N_SECT; k++)
            if (L.sec_best[k] <= 0 || L.sec_cur[k] < L.sec_best[k]) L.sec_best[k] = L.sec_cur[k];
    }
    L.last = lap;
    L.prev_best = L.best;
    bool is_best = L.best <= 0 || lap < L.best || L.n_ref == 0;
    if (L.best <= 0 || lap < L.best) L.best = lap;
    memmove(&L.hist[1], &L.hist[0], sizeof(float) * (LAP_N_HIST - 1));
    L.hist[0] = lap;
    if (L.n_hist < LAP_N_HIST) L.n_hist++;
    L.done++;
    /* parim ring = uus võrdlusring (delta ja kaart) */
    if (is_best && L.n_cur > 20) {
        memcpy(p.ref, p.cur, sizeof(lap_pt_t) * L.n_cur);
        L.n_ref = L.n_cur;
        if (!L.gate[0].set) {
            make_gates();
            save_track(atan2(L.line.dx, L.line.dy) / DEG2RAD);
        }
    }
    logger_mark_lap(L.done, lap);
}

static void try_saved_track(const gps_data_t *g)
{
    p.saved_checked = true;
    saved_track_t t;
    if (!store_load("track", &t, sizeof(t)) || t.ver != 2) return;
    double dy = (g->lat - t.lat0) * 110540.0, dx = (g->lon - t.lon0) * 111320.0 * cos(t.lat0 * DEG2RAD);
    if (dx * dx + dy * dy > AUTO_TRACK_M * AUTO_TRACK_M) return;
    ensure_buffers();
    set_line(t.lat0, t.lon0, t.course);
    memcpy(L.gate, t.gate, sizeof(L.gate));
    L.gates_manual = t.manual;
    L.running = false;          /* ring algab esimesel joone ületamisel */
}

void lap_sample(const dash_state_t *s)
{
    const gps_data_t *g = &s->gps;
    if (!g->fix) {
        p.have_prev = false;
        p.last_sample = g->samples;
        L.pos_ok = false;
        return;
    }
    if (g->samples == p.last_sample) return;
    p.last_sample = g->samples;
    if (g->speed_kmh > 5) p.moving_course = g->course;
    if (!L.line_set && !p.saved_checked) try_saved_track(g);
    if (!L.line_set) return;
    ensure_buffers();
    if (!p.cur || !p.ref) return;

    double x, y;
    to_xy(g->lat, g->lon, &x, &y);
    L.pos_x = (float)x;
    L.pos_y = (float)y;
    L.pos_ok = true;
    int64_t t = g->sample_us;
    if (p.have_prev) {
        double f;
        if (crossed(&L.line, p.px, p.py, x, y, &f)) {
            int64_t tc = p.pt + (int64_t)(f * (double)(t - p.pt));
            float lap = (tc - L.lap_start) / 1e6f;
            if (!L.running || lap >= DASH_LAP_MIN_S) {
                if (L.running) lap_done(tc);
                L.laps++;
                L.lap_start = L.sec_start = tc;
                L.running = true;
                L.sect = 0;
                memset(L.sec_cur, 0, sizeof(L.sec_cur));
                L.n_cur = 0;
                p.cur_d = 0;
                p.ref_idx = 0;
            }
        } else if (L.running && L.sect < LAP_N_SECT - 1 && L.gate[L.sect].set &&
                   crossed(&L.gate[L.sect], p.px, p.py, x, y, &f)) {
            int64_t tc = p.pt + (int64_t)(f * (double)(t - p.pt));
            L.sec_cur[L.sect] = (tc - L.sec_start) / 1e6f;
            L.sec_start = tc;
            L.sect++;
        }
    }
    if (L.running) {
        float tl = (t - L.lap_start) / 1e6f;
        if (L.n_cur > 0) {
            double dx = x - p.cur[L.n_cur - 1].x, dy = y - p.cur[L.n_cur - 1].y;
            p.cur_d += (float)sqrt(dx * dx + dy * dy);
        }
        if (L.n_cur < LAP_TRACE_MAX) p.cur[L.n_cur++] = (lap_pt_t){ (float)x, (float)y, tl, p.cur_d };
        update_delta(x, y, tl);
    }
    p.px = x;
    p.py = y;
    p.pt = t;
    p.have_prev = true;
}
