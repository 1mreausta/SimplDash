/*
 * Dash → MaxxECU: nuppude olek ühes CAN-kaadris (DASH_ECU_BUTTONS, dash_config.h).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "dash_common.h"

int  can_ecu_count(void);
const dash_ecu_btn_t *can_ecu_button(int i);
/* Nupp vajutati (pressed = true) / vabastati (false) */
void can_ecu_press(int i, bool pressed);
/* Nupu olek: TOGGLE/HOLD 0/1, SELECT väärtus */
int  can_ecu_value(int i);
/* Kutsu igas UI tsüklis: saadab kaadri DASH_CAN_TX_HZ korda sekundis (kui seadetes lubatud) */
void can_ecu_update(const dash_state_t *s);
/* Viimane saadetud kaader (diagnostika) ja kas saatmine õnnestus */
void can_ecu_last(uint8_t out[8], bool *ok, uint32_t *count);
