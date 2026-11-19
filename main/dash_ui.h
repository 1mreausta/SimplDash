#pragma once
#include <stdint.h>

typedef struct {
    /* Kutsutakse, kui kasutaja puudutusega heledust vahetab (0–100 %) */
    void (*on_brightness)(uint8_t percent);
    /* Kutsutakse, kui vaade vahetub (nt et see NVS-i meelde jätta). Võib olla NULL. */
    void (*on_page)(int page);
    /* Millise vaatega alustada */
    int start_page;
} dash_ui_cfg_t;

/* Loo dash. Kutsu LVGL lukuga (lvgl_port_lock). */
void dash_ui_create(const dash_ui_cfg_t *cfg);

/* Vaadete arv */
int dash_ui_page_count(void);

/* Vaheta vaadet (+1 järgmine, -1 eelmine). Võib kutsuda mis tahes taskist (nt nupp). */
void dash_ui_request_page_step(int dir);

/* Kiiruse allikas: DASH_SPEED_ECU / DASH_SPEED_GPS / DASH_SPEED_AUTO (dash_config.h) */
void dash_ui_set_speed_source(int src);
int dash_ui_get_speed_source(void);

/* GPS UTC → kohalik kell (seadete ajavöönd + suveaeg) */
#include <stdbool.h>
#include "gps.h"
bool dash_ui_local_time(const gps_data_t *g, int *hh, int *mm);

/* Viimase UI tsükli olek (veebilehe jaoks; loe lukuga) */
struct dash_state_s;
const struct dash_state_s *dash_ui_state(void);

/* Näita käivitusekraani uuesti (simulaator). Kutsu LVGL lukuga. */
void dash_ui_show_boot(void);

/* Ava kindel vaade. Kutsu LVGL lukuga. */
void dash_ui_set_page(int page);
