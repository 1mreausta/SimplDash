/*
 * Sõidulogi SD-kaardile: iga sõit eraldi CSV-faili <juur>/logs/.
 * Read koostatakse UI tsüklis, kirjutamine käib eraldi taskis (SD ei pidurda ekraani).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "dash_common.h"

typedef struct {
    bool     card;          /* kaart on paigaldatud */
    bool     active;        /* fail on avatud ja logib */
    char     file[32];      /* praegune / viimane fail */
    uint32_t rows;
    uint32_t dropped;       /* puhvri ületäitumisel kadunud read */
    bool     error;
} logger_status_t;

/* Plaadil: paigalda kaart ja käivita kirjutaja. Simulaatoris: kaust $SIMPLDASH_DATA/sdcard. */
void logger_start(void);
void logger_update(const dash_state_t *s);
void logger_status(logger_status_t *out);
/* Logikausta tee (nt "/sdcard/logs"); NULL kui kaarti pole */
const char *logger_dir(void);
/* SD-kaardi maht baitides; false = kaarti pole */
bool logger_space(uint64_t *total, uint64_t *free_b);
/* Kirjutuskiiruse test (256 kB faili kirjutamine ja kustutamine): kB/s, < 0 = viga */
float logger_speed_test(void);
/* Ringi lõpp (RING vaade) – märgitakse logisse */
void logger_mark_lap(int lap, float time_s);
