#include "demo.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_random.h"
#include "gps.h"
#include "gps_pages.h"
#include "maxxecu.h"
#ifndef DASH_HOST
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

static volatile bool s_on, s_hot, s_gps;

void demo_set_enabled(bool on) { s_on = on; }
bool demo_is_enabled(void) { return s_on; }
#ifdef DASH_HOST
bool demo_skip_records(void) { return false; }   /* simulaatoris on kõik demo – näita logisid ikkagi */
#else
bool demo_skip_records(void) { return s_on; }
#endif
void demo_set_overheat(bool on) { s_hot = on; }
bool demo_is_overheat(void) { return s_hot; }

/* --- võlts-GPS: auto sõidab ovaalsel rajal, NMEA laused lähevad päris parserisse --- */
/* Demo-rada: suletud kõver (Catmull-Rom) läbi nende punktide (meetrid: ida, põhi), ~2,3 km.
 * Pikk stardisirge põhja, kiire parem kurv, juuspööre, šikaan, pikk kaar, teine juuspööre ja S-kurvid. */
static const float WP[][2] = {
    { 0, 0 },     { 0, 350 },   { 40, 430 },   { 140, 450 }, { 230, 410 },  { 250, 330 },
    { 200, 270 }, { 250, 210 }, { 360, 190 },  { 430, 120 }, { 420, 20 },   { 340, -40 },
    { 250, -20 }, { 200, 60 },  { 130, 40 },   { 90, -60 },  { 40, -110 },  { 0, -80 },
};
#define N_WP   ((int)(sizeof(WP) / sizeof(WP[0])))
#define SUB    40                      /* punkte lõigu kohta */
#define N_TP   (N_WP * SUB)
#define TRACK_LAT0 59.3950             /* stardijoone asukoht */
#define TRACK_LON0 24.6650
static float s_tx[N_TP + 1], s_ty[N_TP + 1], s_td[N_TP + 1];   /* punktid ja teekond algusest */
static double s_dist;          /* läbitud teekond rajal (m) */
static time_t s_gps_t0;
static float  s_gps_since;

static void track_build(void)
{
    if (s_td[N_TP] > 0) return;
    int n = 0;
    for (int k = 0; k < N_WP; k++) {
        const float *p0 = WP[(k - 1 + N_WP) % N_WP], *p1 = WP[k], *p2 = WP[(k + 1) % N_WP], *p3 = WP[(k + 2) % N_WP];
        for (int i = 0; i < SUB; i++) {
            float t = (float)i / SUB, t2 = t * t, t3 = t2 * t;
            for (int c = 0; c < 2; c++) {
                float v = 0.5f * (2 * p1[c] + (-p0[c] + p2[c]) * t + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t2 +
                                  (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t3);
                (c ? s_ty : s_tx)[n] = v;
            }
            n++;
        }
    }
    s_tx[N_TP] = s_tx[0];
    s_ty[N_TP] = s_ty[0];
    s_td[0] = 0;
    for (int i = 1; i <= N_TP; i++)
        s_td[i] = s_td[i - 1] + hypotf(s_tx[i] - s_tx[i - 1], s_ty[i] - s_ty[i - 1]);
}

/* asukoht (m, ida/põhi) ja sõidusuund raja teekonna järgi; algus = start/finiš */
static void track_pos(double d, double *x, double *y, double *course)
{
    track_build();
    const double L = s_td[N_TP];
    d = fmod(d, L);
    if (d < 0) d += L;
    int i = 0;
    while (i < N_TP - 1 && s_td[i + 1] < d) i++;
    double seg = s_td[i + 1] - s_td[i], f = seg > 0 ? (d - s_td[i]) / seg : 0;
    double dx = s_tx[i + 1] - s_tx[i], dy = s_ty[i + 1] - s_ty[i];
    *x = s_tx[i] + f * dx;
    *y = s_ty[i] + f * dy;
    *course = fmod(atan2(dx, dy) * 180 / M_PI + 360, 360);
}

void demo_set_gps(bool on)
{
    s_gps = on;
    if (on) {
        s_gps_t0 = time(NULL);
        s_gps_since = -1;
        /* demo: start/finiš joon raja algusesse (stardisirge, sõit põhja) → ringid loetakse ise */
        double x, y, c;
        track_pos(0, &x, &y, &c);
        lap_set_line(TRACK_LAT0 + y / 110540.0, TRACK_LON0 + x / (111320.0 * cos(TRACK_LAT0 * M_PI / 180)), (float)c,
                     false);
        /* ja paar "varasemat" demotulemust, et vaated poleks alguses tühjad */
        static const float laps[] = { 71.42f, 69.87f, 72.05f };
        static const float accel[] = { 5.06f, 4.97f, 5.12f };
        lap_seed_history(laps, 3);
        accel_seed_history(accel, 3, 2.31f);
    } else {
        gps_reset();
    }
}
bool demo_is_gps(void) { return s_gps; }

static void nmea_send(char *body)
{
    uint8_t sum = 0;
    for (char *c = body; *c; c++) sum ^= (uint8_t)*c;
    char line[192];
    int n = snprintf(line, sizeof(line), "$%s*%02X\r\n", body, sum);
    if (n < 0 || n >= (int)sizeof(line)) return;
    gps_feed(line, (size_t)n);
}

static void gps_emit(float t_now, float speed_kmh)
{
    if (s_gps_since < 0) s_gps_since = t_now;
    double x, y, course;
    track_pos(s_dist, &x, &y, &course);
    double lat = TRACK_LAT0 + y / 110540.0;
    double lon = TRACK_LON0 + x / (111320.0 * cos(TRACK_LAT0 * M_PI / 180));
    double secs = (double)s_gps_t0 + (t_now - s_gps_since);
    time_t ts = (time_t)secs;
    struct tm tm;
    gmtime_r(&ts, &tm);
    int cs = (int)((secs - (double)ts) * 100);
    char hms[32], date[24], la[24], lo[24], body[176];
    snprintf(hms, sizeof(hms), "%02d%02d%02d.%02d", tm.tm_hour, tm.tm_min, tm.tm_sec, cs);
    snprintf(date, sizeof(date), "%02d%02d%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year % 100);
    int ld = (int)lat, od = (int)lon;
    snprintf(la, sizeof(la), "%02d%08.5f", ld, (lat - ld) * 60);
    snprintf(lo, sizeof(lo), "%03d%08.5f", od, (lon - od) * 60);
    snprintf(body, sizeof(body), "GNRMC,%s,A,%s,N,%s,E,%.2f,%.1f,%s,,,A", hms, la, lo, speed_kmh / 1.852, course, date);
    nmea_send(body);
    snprintf(body, sizeof(body), "GNGGA,%s,%s,N,%s,E,1,14,0.7,42.0,M,18.0,M,,", hms, la, lo);
    nmea_send(body);
}

static float rnd(float a, float b)
{
    return a + (b - a) * (esp_random() / 4294967295.0f);
}

static const float RATIOS[6] = { 3.36f, 2.07f, 1.43f, 1.10f, 0.87f, 0.73f };

static float rpm_for(float kmh, int gear)
{
    return kmh / 3.6f / 1.95f * RATIOS[gear - 1] * 3.9f * 60.0f;
}

/* sõidumudeli olek */
static float v[SIG_COUNT];
static float speed = 0, clt = 40, oil_t = 35, tank = 45, t = 0, idle_since = 0, knock_until = 0;
static int gear = 1, phase = 0; /* 0 kiirendus, 1 pidurdus, 2 tühikäik */
static int tick = 0;

static volatile bool s_knock;
static float s_knock_next;
void demo_set_knock(bool on) { s_knock = on; s_knock_next = 0; }
bool demo_is_knock(void) { return s_knock; }

/* Üks 20 ms samm: arvuta mudel ja saada kaadrid dekoodrisse nagu päris ECU */
void demo_step(void)
{
    const float dt = 0.02f;
    {
        t += dt;
        if (s_knock && t >= s_knock_next) { knock_until = t + 0.3f; s_knock_next = t + 2.5f; }
        bool shiftcut = false;
        float tps, rpm;

        if (phase == 0) {
            speed += 26.0f / powf(gear, 0.9f) * dt;
            if (rpm_for(speed, gear) >= 7500 && gear < 6) { gear++; shiftcut = true; }
            else if (gear == 6 && speed > 230) { phase = 1; }
            tps = 100;
        } else if (phase == 1) {
            speed -= 38 * dt;
            tps = 0;
            if (rpm_for(speed, gear) < 2600 && gear > 1) { gear--; }
            if (speed <= 5) { speed = 0; phase = 2; idle_since = t; }
        } else {
            tps = 2;
            if (t - idle_since > 2.5f) { phase = 0; gear = 1; }
        }
        rpm = fmaxf(950, rpm_for(speed, gear));
        if (phase == 2) { rpm = 950 + 30 * sinf(t * 3); }
        rpm = fminf(rpm, 7800);

        float load = tps / 100;
        float map = 35 + load * fminf(1, rpm / 4500) * 175 + rnd(-1, 1);
        float clt_norm = 88 + 8 * load;
        if (s_hot) clt += (116 - clt) * 0.5f * dt;              /* ülekuumenemise test */
        else if (clt > clt_norm + 3) clt += (clt_norm - clt) * 0.8f * dt;  /* test läbi: jahtub kiiresti */
        else clt += (clt_norm - clt) * 0.02f * dt;
        oil_t += ((105 + 15 * load) - oil_t) * 0.01f * dt;
        tank = fmaxf(0, tank - load * 0.004f * dt);
        float lam_t = load > 0.6f ? 0.80f : 1.0f;
        float egt = 380 + load * 380 + rpm / 40;

        v[SIG_RPM] = rpm; v[SIG_TPS] = tps; v[SIG_MAP] = map; v[SIG_LAMBDA] = lam_t + rnd(-0.01f, 0.01f);
        v[SIG_LAMBDA_A] = v[SIG_LAMBDA]; v[SIG_LAMBDA_B] = v[SIG_LAMBDA] + 0.005f;
        v[SIG_IGN_ANGLE] = 30 - load * 14; v[SIG_IGN_CUT] = shiftcut ? 100 : 0;
        v[SIG_FUEL_PW] = 1.8f + load * 9; v[SIG_FUEL_DUTY] = fminf(95, rpm / 7800 * load * 90 + 2);
        v[SIG_SPEED] = speed; v[SIG_UNDRIVEN_SPEED] = speed; v[SIG_DRIVEN_SPEED] = speed;
        v[SIG_FW_VERSION] = 1.135f; v[SIG_REVLIMIT_RPM] = 7800; v[SIG_LAMBDA_TARGET] = lam_t;
        v[SIG_ACC_FWD] = phase == 0 ? 0.6f / gear : -1.1f; v[SIG_ACC_UP] = 1;
        v[SIG_BATT] = 14.1f + rnd(-0.05f, 0.05f); v[SIG_BARO] = 101.3f;
        v[SIG_IAT] = 28 + load * 12; v[SIG_CLT] = clt;
        v[SIG_FUEL_TRIM] = rnd(-2, 2); v[SIG_ETHANOL] = 0;
        v[SIG_EGT1] = egt; v[SIG_EGT2] = egt - 10; v[SIG_EGT3] = egt + 8; v[SIG_EGT4] = egt - 4;
        v[SIG_EGT_MAX] = egt + 8; v[SIG_EGT_DIFF] = 18; v[SIG_ECU_TEMP] = 42;
        v[SIG_GEAR] = (phase == 2) ? 0 : gear; v[SIG_BOOST_DUTY] = load * 65;
        v[SIG_OIL_PRESS] = 150 + rpm / 7800 * 380; v[SIG_OIL_TEMP] = oil_t;
        v[SIG_FUEL_PRESS] = 300 + (map - 101.3f); v[SIG_BOOST_TARGET] = 220;
        v[SIG_FUEL_TANK] = tank;

        uint16_t flags = FLAG_LOGGING;
        if (shiftcut) flags |= FLAG_SHIFTCUT;
        if (rpm >= 7795) flags |= FLAG_REVLIMIT;
        if (load > 0.9f && gear == 1) flags |= FLAG_LAUNCH;
        if (phase == 1 && rpm > 3500) flags |= FLAG_ANTILAG;  /* gaas maas, ALS hoiab turbot */
        if (t < knock_until) flags |= FLAG_KNOCK;
        if (phase == 1) flags |= FLAG_BRAKE;

        /* Saada kaadrid samade sagedustega nagu päris ECU: 0x52x 50 Hz, ülejäänud 10 Hz */
        uint8_t buf[8];
        for (const uint16_t *id = maxxecu_frame_ids; *id; id++) {
            if (*id < 0x530 || tick % 5 == 0) {
                maxxecu_encode(*id, v, flags, buf);
                maxxecu_decode(*id, buf, 8);
            }
        }
        /* GPS: teekond rajal + 10 Hz NMEA */
        s_dist += speed / 3.6f * dt;
        if (s_gps && tick % 5 == 0) gps_emit(t, speed + rnd(-0.15f, 0.15f) * (speed > 1));
        tick++;
    }
}

#ifndef DASH_HOST
static void demo_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (s_on) demo_step();
    }
}

void demo_start_task(void)
{
    xTaskCreate(demo_task, "demo", 4096, NULL, 5, NULL);
}
#endif
