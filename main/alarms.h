/*
 * Hoiatused: piirid (seadetest), õlirõhu kõver pöörete järgi, ECU olekuhoiatused
 * (KNOCK, veakood), täisekraani hoiatus ja hoiatuste logi (püsimälus).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "dash_common.h"

typedef struct {
    uint32_t uptime_s;      /* sekundit käivitusest */
    uint32_t engine_s;      /* mootoritunnid sekundites (stats) */
    uint32_t odo_m;         /* läbisõit meetrites */
    int16_t  year;          /* GPS kuupäev/kell (UTC), year = 0 → pole */
    int8_t   mon, day, hour, min, sec;
    int8_t   kind;          /* 0 piir, 1 ECU */
    int8_t   idx;           /* DASH_WARNINGS / DASH_ECU_ALERTS indeks */
    float    value;         /* signaali väärtus (SI) */
    float    limit;         /* piir (SI), NAN kui pole */
    int16_t  rpm;
} alarm_event_t;

/* Arvuta hoiatused (täidab s->warn, s->warn_txt, s->alert_*). Kutsu igas UI tsüklis. */
void alarms_eval(dash_state_t *s);
/* Täisekraani hoiatus kinnitatud (puudutus): sama hoiatus ei kata ekraani 60 s */
void alarms_ack(void);
/* Õlirõhu miinimum (kPa) antud pööretel */
float alarms_oil_min(float rpm);

void alarms_init(void);
int  alarms_log_count(void);
/* i = 0 on uusim */
const alarm_event_t *alarms_log_get(int i);
void alarms_log_clear(void);
/* Hoiatuse nimi logikirjest */
const char *alarms_event_label(const alarm_event_t *e);
/* Mitu hoiatust on käesoleval sõidul logisse lisandunud */
int alarms_new_count(void);
