/* GPS-vaadete (0–100, RING) liides demo/simulaatori jaoks. */
#pragma once
#include <stdbool.h>

/* RING: määra start/finiš joon (asukoht, sõidusuund kraadides).
 * start_now = ring algab kohe; muidu esimesel joone ületamisel. */
void lap_set_line(double lat, double lon, float course_deg, bool start_now);
/* RING: lisa varasemad ringiajad (s), uuem enne; ainult kui ajalugu on tühi */
void lap_seed_history(const float *laps, int n);
/* 0–100: lisa varasemad tulemused (s), uuem enne, ja 0–60 vaheaeg; ainult kui ajalugu on tühi */
void accel_seed_history(const float *times, int n, float split);
