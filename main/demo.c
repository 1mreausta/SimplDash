#include "demo.h"
#include <math.h>
#include "esp_random.h"
#include "maxxecu.h"
#ifndef DASH_HOST
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

static volatile bool s_on, s_hot;

void demo_set_enabled(bool on) { s_on = on; }
bool demo_is_enabled(void) { return s_on; }
void demo_set_overheat(bool on) { s_hot = on; }
bool demo_is_overheat(void) { return s_hot; }

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

/* Üks 20 ms samm: arvuta mudel ja saada kaadrid dekoodrisse nagu päris ECU */
void demo_step(void)
{
    const float dt = 0.02f;
    {
        t += dt;
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
        if (load > 0.9f && rpm > 6500 && esp_random() % 500 == 0) { knock_until = t + 0.3f; }
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
