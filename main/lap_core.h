/*
 * Ringiaja arvestus (RING ja RADA vaated): start/finiš joon, ringid, sektorid,
 * vahe parima ringiga (delta) ja raja kuju. Raja joon ja sektorid jäetakse meelde.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "dash_common.h"

#define LAP_N_SECT   3
#define LAP_TRACE_MAX 3000       /* punkte ringi kohta (10 Hz → 5 min ring) */
#define LAP_N_HIST   5

typedef struct { float x, y, t, d; } lap_pt_t;    /* meetrid joone suhtes (ida, põhi), aeg ja teekond ringi algusest */
typedef struct { double cx, cy, dx, dy; bool set; } lap_gate_t;

typedef struct {
    bool     line_set, running;
    int      laps, done;                  /* käimasolev ring, läbitud ringe */
    float    last, best, prev_best;
    float    hist[LAP_N_HIST];
    int      n_hist;
    int64_t  lap_start;
    /* delta parima ringiga */
    bool     delta_ok;
    float    delta;
    /* sektorid */
    int      sect;                        /* käimasolev sektor 0..2 */
    float    sec_cur[LAP_N_SECT];         /* käimasoleva ringi sektorid (0 = pole veel) */
    float    sec_last[LAP_N_SECT], sec_best[LAP_N_SECT];
    int64_t  sec_start;
    /* rada */
    const lap_pt_t *ref;                  /* parim ring (või esimene) */
    int      n_ref;
    const lap_pt_t *cur;
    int      n_cur;
    float    pos_x, pos_y;
    bool     pos_ok;
    lap_gate_t line, gate[LAP_N_SECT - 1];
    bool     gates_manual;                /* sektorid on käsitsi määratud */
} lap_info_t;

const lap_info_t *lap_info(void);
/* GPS-i uus mõõtmine (kutsub RING vaate background) */
void lap_sample(const dash_state_t *s);
/* Uus start/finiš joon (kustutab raja kuju ja sektorid) */
void lap_mark_line(void);
/* Nulli ajad ja parim ring (joon jääb) */
void lap_reset_times(void);
/* Sektori k (0 = S1 lõpp, 1 = S2 lõpp) piir lähimasse raja punkti (x, y meetrites).
 * false = raja kuju veel pole või punkt on rajast kaugel (> 60 m). Sektorite ajad nullitakse. */
bool lap_set_gate(int k, float x, float y);
/* Sektorid tagasi automaatseks (raja pikkuse kolmandikud) */
void lap_auto_gates(void);
