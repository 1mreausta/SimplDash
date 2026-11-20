/*
 * Läbisõit, mootoritunnid ja reis (läbisõit, kütus, keskmine kulu). Püsimälus.
 * Kütus arvutatakse pihustite töötsüklist (seaded: pihusti cc/min × silindrid).
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "dash_common.h"

typedef struct {
    uint16_t ver;
    double   odo_m;          /* koguläbisõit (alates dashi paigaldusest) */
    uint32_t engine_s;       /* mootoritunnid sekundites */
    double   trip_m;
    float    trip_fuel_l;
    uint32_t trip_s;         /* mootori töötamise aeg reisil */
    /* UUED VÄLJAD SIIA (lõppu) ja uuenda STATS_DATA_END */
} dash_stats_t;
#define STATS_DATA_END (offsetof(dash_stats_t, trip_s) + sizeof(uint32_t))   /* ilma lõpu täiteta, vt SET_DATA_END */

void stats_init(void);
void stats_update(const dash_state_t *s);
const dash_stats_t *stats_get(void);
void stats_trip_reset(void);
/* Hetke kütusekulu L/h (pihustite töötsüklist); < 0 kui andmeid pole */
float stats_fuel_lph(const dash_state_t *s);
/* Keskmine kulu L/100km reisil; < 0 kui reis alla 1 km */
float stats_trip_avg(void);
/* Hetke kulu: liikudes L/100km (*per_km = true), seistes L/h; < 0 kui andmeid pole */
float stats_fuel_now(const dash_state_t *s, bool *per_km);
/* Seadmine (nt veebilehelt: odomeetri algväärtus); false = vigane väärtus */
bool stats_set_odo_km(double km);
