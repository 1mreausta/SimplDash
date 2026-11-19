/*
 * Diagnostika esimeseks paigalduseks (SEADED → DIAG): CAN-kaadrid ID-de kaupa (sagedus, viimane),
 * CAN-i vead, GPS-i toorlaused, SD-kaardi maht ja kiirus, süsteemi olek.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DIAG_MAX_IDS   48
#define DIAG_GPS_LINES 5

typedef struct {
    uint16_t id;
    uint8_t  dlc;
    uint32_t count;
    float    hz;          /* viimase sekundi sagedus */
    int64_t  last_us;
} diag_id_t;

/* --- kogumine (kutsuvad teised moodulid, lõimekindel) --- */
void diag_can_rx(uint32_t id, uint8_t dlc);   /* iga vastuvõetud kaader */
void diag_can_busoff(void);
void diag_can_overflow(void);                 /* vastuvõtu järjekord täis (kaader kadus) */
void diag_gps_line(const char *line, bool checksum_ok);
/* CAN kontrolleri olek (plaadil; state 0 aktiivne, 1 hoiatus, 2 passiivne, 3 bus-off; -1 = CAN ei käivitunud) */
void diag_can_state(int state, int tec, int rec, uint32_t bus_errors, bool listen_only);

/* --- lugemine (UI) --- */
void     diag_tick(int64_t now_us);           /* arvutab sagedused (kutsu tihti, teeb tööd kord sekundis) */
int      diag_can_ids(diag_id_t *out, int max);   /* ID järgi sorteeritud */
uint32_t diag_can_total(void);
uint32_t diag_can_busoffs(void);
uint32_t diag_can_overflows(void);
/* CAN kontrolleri olek tekstina ("" simulaatoris) */
void     diag_can_state_text(char *buf, size_t cap);
float    diag_can_total_hz(void);
int      diag_gps_lines(char out[][100], int max);  /* uusim esimesena */
uint32_t diag_gps_good(void);
uint32_t diag_gps_bad(void);
/* süsteemi olek tekstina (mälu, tööaeg, taaskäivituse põhjus) */
void     diag_system(char *buf, size_t cap);
