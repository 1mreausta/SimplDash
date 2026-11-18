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

/* Ava kindel vaade. Kutsu LVGL lukuga. */
void dash_ui_set_page(int page);
