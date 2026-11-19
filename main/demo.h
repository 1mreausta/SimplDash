#pragma once
#include <stdbool.h>

/* Demorežiim: genereerib MaxxECU kaadreid ja saadab need läbi sama dekoodri. */
void demo_set_enabled(bool on);
bool demo_is_enabled(void);
void demo_start_task(void);
/* Üks 20 ms samm sõidumudelist (demo_start_task kutsub seda ise) */
void demo_step(void);
/* Test: vesi kuumeneb üle (hoiatuse proovimiseks) */
void demo_set_overheat(bool on);
bool demo_is_overheat(void);
/* Test: võlts-GPS (ovaalne rada, 10 Hz NMEA) – simulaatoris */
void demo_set_gps(bool on);
bool demo_is_gps(void);
