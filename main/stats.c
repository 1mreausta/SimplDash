#include "stats.h"
#include <string.h>
#include "demo.h"
#include "settings.h"
#include "store.h"

static dash_stats_t st;
static int64_t s_last_us, s_saved_us;
static bool s_dirty;

void stats_init(void)
{
    memset(&st, 0, sizeof(st));
    st.ver = 1;
    dash_stats_t c = st;   /* vanemas versioonis puuduvad väljad jäävad nulliks */
    if (store_load_grow("stats", &c, sizeof(c)) && c.ver == 1) st = c;
}

const dash_stats_t *stats_get(void) { return &st; }

static void save(int64_t now)
{
    store_save("stats", &st, STATS_DATA_END);
    s_saved_us = now;
    s_dirty = false;
}

void stats_trip_reset(void)
{
    st.trip_m = 0;
    st.trip_fuel_l = 0;
    st.trip_s = 0;
    s_dirty = true;
    save(s_last_us);
}

void stats_set_odo_km(double km)
{
    st.odo_m = km * 1000.0;
    save(s_last_us);
}

float stats_fuel_lph(const dash_state_t *s)
{
    if (s->stale || !s->d.valid[SIG_FUEL_DUTY] || s->rpm < 300) return -1;
    return g_set.inj_cc * g_set.cylinders * s->d.v[SIG_FUEL_DUTY] / 100.0f * 60.0f / 1000.0f;
}

void stats_update(const dash_state_t *s)
{
    int64_t now = s->now_us;
    if (demo_skip_records()) {   /* demosõit ei loe läbisõitu, tunde ega kütust */
        s_last_us = now;
        return;
    }
    if (!s_last_us) {
        s_last_us = s_saved_us = now;
        return;
    }
    float dt = (now - s_last_us) / 1e6f;
    s_last_us = now;
    if (dt <= 0 || dt > 1.0f) return;   /* paus (nt simulaator peatatud) */

    if (!s->stale && s->rpm > DASH_ENGINE_RUN_RPM) {
        static float frac;
        frac += dt;
        if (frac >= 1) {
            st.engine_s += (uint32_t)frac;
            st.trip_s += (uint32_t)frac;
            frac -= (uint32_t)frac;
        }
        float lph = stats_fuel_lph(s);
        if (lph > 0) st.trip_fuel_l += lph * dt / 3600.0f;
        s_dirty = true;
    }
    if (s->d.valid[SIG_SPEED] && s->d.v[SIG_SPEED] > 1.0f) {
        double d = s->d.v[SIG_SPEED] / 3.6 * dt;
        st.odo_m += d;
        st.trip_m += d;
        s_dirty = true;
    }
    /* salvesta perioodiliselt ja kohe, kui auto peatub (süüde võidakse kohe välja lülitada) */
    static bool moving;
    bool mv = s->d.valid[SIG_SPEED] && s->d.v[SIG_SPEED] > 1.0f;
    if (s_dirty && (now - s_saved_us > (int64_t)DASH_STATS_SAVE_S * 1000000 || (moving && !mv))) save(now);
    moving = mv;
}
