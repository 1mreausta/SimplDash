#pragma once
#include "esp_err.h"

/* Käivita TWAI (CAN) vastuvõtt; kaadrid dekodeeritakse maxxecu_decode() kaudu. */
esp_err_t can_rx_start(void);
