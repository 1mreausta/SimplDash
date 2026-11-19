/*
 * Veebilehe API (ühine plaadi Wi-Fi serverile ja simulaatorile):
 *   /            leht (web_page.html)
 *   /api/status  olek (JSON)        /api/settings  seaded (JSON)
 *   /api/set?key=..&val=..          /api/alarms    hoiatuste logi
 *   /api/logs    logifailid         /logs/<fail>   allalaadimine   /api/del?f=..
 *   /api/trip_reset  /api/odo?km=..  /api/alarms_clear  /api/defaults
 * Püsivara uuendus (POST /ota) ja taaskäivitus on ainult plaadil (wifi_web.c).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int    status;           /* 200, 404 ... */
    const char *type;
    const char *body;        /* vastuse sisu (staatiline puhver) */
    size_t len;
    char   file[160];        /* kui täidetud: saada see fail (allalaadimine) */
    char   name[48];         /* faili nimi allalaadimisel */
} web_resp_t;

/* Töötle päring (uri koos ?query-ga). Seadete muutmine peab olema lõimekindel – vt web_lock. */
void web_api_handle(const char *uri, web_resp_t *r);

/* Platvorm: lukk UI ja veebi vahel (plaadil LVGL lukk, simulaatoris tühi) */
void web_lock(void);
void web_unlock(void);
/* Platvorm: lisainfo olekusse (nt Wi-Fi kliendid), võib olla tühi */
void web_platform_status(char *buf, size_t cap);

extern const char web_page_html[];
