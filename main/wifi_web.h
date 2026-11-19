/*
 * Wi-Fi pääsupunkt (plaadi ESP32-C6) ja veebileht http://192.168.4.1:
 * seaded, logifailid, hoiatuste logi, püsivara uuendus (OTA).
 */
#pragma once
#include <stdbool.h>

/* Käivita (kui seadetes lubatud). Plaadil; simulaatoris ei tee midagi. */
void wifi_web_start(void);
/* Olek tekstina SEADED vaate jaoks */
void wifi_web_status(char *buf, int cap);
