#include "lamps.h"
#include <stdbool.h>
#include "esp_timer.h"
#ifndef DASH_HOST
#include "driver/gpio.h"
#include "sdkconfig.h"
static const int PINS[3] = { CONFIG_SIMPLDASH_LAMP_LEFT_GPIO, CONFIG_SIMPLDASH_LAMP_RIGHT_GPIO,
                             CONFIG_SIMPLDASH_LAMP_HIGH_GPIO };
#endif

static volatile uint8_t s_test;

void lamps_set_test(uint8_t bits) { s_test = bits; }
uint8_t lamps_test(void) { return s_test; }

void lamps_start(void)
{
#ifndef DASH_HOST
    for (int i = 0; i < 3; i++) {
        if (PINS[i] < 0) continue;
        gpio_config_t io = { .pin_bit_mask = 1ULL << PINS[i], .mode = GPIO_MODE_INPUT,
                             .pull_up_en = GPIO_PULLUP_ENABLE };
        gpio_config(&io);
    }
#endif
}

uint8_t lamps_get(void)
{
    uint8_t l = 0;
#ifndef DASH_HOST
    for (int i = 0; i < 3; i++) {
        if (PINS[i] >= 0 && gpio_get_level(PINS[i]) == 0) l |= 1u << i;
    }
#endif
    const uint8_t t = s_test;
    const bool on = (esp_timer_get_time() / 333000) % 2 == 0;   /* nagu suunatule relee */
    return l | (t & LAMP_HIGH) | (on ? t & (LAMP_LEFT | LAMP_RIGHT) : 0);
}
