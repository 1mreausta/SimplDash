/*
 * Tänavarežiimi märgutuled: suunatuled ja kaugtuled otse plaadi GPIO-desse (optroni kaudu).
 * Sisend on aktiivne madal: tuli põleb → optron tõmbab GPIO maaga (GND); sisemine pull-up on sees.
 * Pinnid: idf.py menuconfig → SimplDash → suunatuled / kaugtuled (-1 = pole ühendatud).
 */
#pragma once
#include <stdint.h>

enum { LAMP_LEFT = 1u << 0, LAMP_RIGHT = 1u << 1, LAMP_HIGH = 1u << 2 };

/* Seadista GPIO-d (plaadil; arvutis ei tee midagi) */
void lamps_start(void);
/* Praegu põlevad tuled (LAMP_* bitid): GPIO-d + simulaatori test */
uint8_t lamps_get(void);
/* Simulaatori test: LAMP_* bitid; suunatuled vilguvad ~1,5 Hz nagu relee */
void lamps_set_test(uint8_t bits);
uint8_t lamps_test(void);
