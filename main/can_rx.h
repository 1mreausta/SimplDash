#pragma once
#include "esp_err.h"

/* Käivita TWAI (CAN) vastuvõtt; kaadrid dekodeeritakse maxxecu_decode() kaudu. */
esp_err_t can_rx_start(void);

#include <stdbool.h>
#include <stdint.h>
/* Saada standardkaader (11-bit ID). false = CAN pole käivitatud, listen-only või järjekord täis. */
bool can_send(uint32_t id, const uint8_t *data, uint8_t len);
