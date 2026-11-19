#include "maxxecu.h"
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "dash_config.h"
#include "diag.h"

typedef enum { T_I16, T_U16, T_U8 } ftype_t;

typedef struct {
    uint16_t id;
    uint8_t  off;
    uint8_t  type;
    float    scale;
    uint8_t  sig;
} field_t;

#define F(id, off, t, sc, s) { id, off, t, sc, s }

static const field_t FIELDS[] = {
    /* 50 Hz */
    F(0x520, 0, T_I16, 1,     SIG_RPM),       F(0x520, 2, T_I16, 0.1f,  SIG_TPS),
    F(0x520, 4, T_I16, 0.1f,  SIG_MAP),       F(0x520, 6, T_I16, 0.001f, SIG_LAMBDA),
    F(0x521, 0, T_I16, 0.001f, SIG_LAMBDA_A), F(0x521, 2, T_I16, 0.001f, SIG_LAMBDA_B),
    F(0x521, 4, T_I16, 0.1f,  SIG_IGN_ANGLE), F(0x521, 6, T_I16, 1,     SIG_IGN_CUT),
    F(0x522, 0, T_I16, 0.01f, SIG_FUEL_PW),   F(0x522, 2, T_I16, 0.1f,  SIG_FUEL_DUTY),
    F(0x522, 4, T_I16, 1,     SIG_FUEL_CUT),  F(0x522, 6, T_I16, 0.1f,  SIG_SPEED),
    F(0x523, 0, T_I16, 0.1f,  SIG_UNDRIVEN_SPEED), F(0x523, 2, T_I16, 0.1f, SIG_DRIVEN_SPEED),
    F(0x523, 4, T_I16, 0.1f,  SIG_WHEEL_SLIP), F(0x523, 6, T_I16, 0.1f, SIG_TARGET_SLIP),
    F(0x524, 0, T_I16, 0.1f,  SIG_TC_POWER_LIMIT), F(0x524, 2, T_I16, 0.1f, SIG_LAMBDA_CORR_A),
    F(0x524, 4, T_I16, 0.1f,  SIG_LAMBDA_CORR_B),  F(0x524, 6, T_I16, 0.001f, SIG_FW_VERSION),
    F(0x525, 0, T_I16, 0.1f,  SIG_USER_CH9),  F(0x525, 2, T_I16, 0.1f,  SIG_USER_CH10),
    F(0x525, 4, T_I16, 0.1f,  SIG_USER_CH11), F(0x525, 6, T_I16, 0.1f,  SIG_USER_CH12),
    F(0x526, 4, T_I16, 1,     SIG_REVLIMIT_RPM),
    F(0x527, 0, T_I16, 0.01f, SIG_ACC_FWD),   F(0x527, 2, T_I16, 0.01f, SIG_ACC_RIGHT),
    F(0x527, 4, T_I16, 0.01f, SIG_ACC_UP),    F(0x527, 6, T_I16, 0.001f, SIG_LAMBDA_TARGET),
    F(0x528, 0, T_I16, 1,     SIG_KNOCK_PEAK), F(0x528, 2, T_I16, 0.1f, SIG_KNOCK_CORR),
    F(0x528, 4, T_I16, 1,     SIG_KNOCK_COUNT), F(0x528, 6, T_I16, 1,   SIG_KNOCK_LAST_CYL),
    /* 10 Hz */
    F(0x530, 0, T_I16, 0.01f, SIG_BATT),      F(0x530, 2, T_I16, 0.1f,  SIG_BARO),
    F(0x530, 4, T_I16, 0.1f,  SIG_IAT),       F(0x530, 6, T_I16, 0.1f,  SIG_CLT),
    F(0x531, 0, T_I16, 0.1f,  SIG_FUEL_TRIM), F(0x531, 2, T_I16, 0.1f,  SIG_ETHANOL),
    F(0x531, 4, T_I16, 0.1f,  SIG_IGN_COMP),  F(0x531, 6, T_I16, 1,     SIG_EGT1),
    F(0x532, 0, T_I16, 1, SIG_EGT2), F(0x532, 2, T_I16, 1, SIG_EGT3),
    F(0x532, 4, T_I16, 1, SIG_EGT4), F(0x532, 6, T_I16, 1, SIG_EGT5),
    F(0x533, 0, T_I16, 1, SIG_EGT6), F(0x533, 2, T_I16, 1, SIG_EGT7),
    F(0x533, 4, T_I16, 1, SIG_EGT8), F(0x533, 6, T_I16, 1, SIG_EGT_MAX),
    F(0x534, 0, T_I16, 1, SIG_EGT_DIFF),     F(0x534, 2, T_I16, 1, SIG_ECU_TEMP),
    F(0x534, 4, T_I16, 1, SIG_ERROR_COUNT),  F(0x534, 6, T_I16, 1, SIG_LOST_SYNC),
    F(0x535, 0, T_I16, 0.1f, SIG_AIN1), F(0x535, 2, T_I16, 0.1f, SIG_AIN2),
    F(0x535, 4, T_I16, 0.1f, SIG_AIN3), F(0x535, 6, T_I16, 0.1f, SIG_AIN4),
    F(0x536, 0, T_I16, 1,    SIG_GEAR),       F(0x536, 2, T_I16, 0.1f, SIG_BOOST_DUTY),
    F(0x536, 4, T_I16, 0.1f, SIG_OIL_PRESS),  F(0x536, 6, T_I16, 0.1f, SIG_OIL_TEMP),
    F(0x537, 0, T_I16, 0.1f, SIG_FUEL_PRESS), F(0x537, 2, T_I16, 0.1f, SIG_WG_PRESS),
    F(0x537, 4, T_I16, 0.1f, SIG_CLT_PRESS),  F(0x537, 6, T_I16, 0.1f, SIG_BOOST_TARGET),
    F(0x538, 0, T_I16, 0.1f, SIG_USER_CH1), F(0x538, 2, T_I16, 0.1f, SIG_USER_CH2),
    F(0x538, 4, T_I16, 0.1f, SIG_USER_CH3), F(0x538, 6, T_I16, 0.1f, SIG_USER_CH4),
    F(0x539, 0, T_I16, 0.1f, SIG_USER_CH5), F(0x539, 2, T_I16, 0.1f, SIG_USER_CH6),
    F(0x539, 4, T_I16, 0.1f, SIG_USER_CH7), F(0x539, 6, T_I16, 0.1f, SIG_USER_CH8),
    F(0x540, 0, T_U8,  1,    SIG_BOOST_TABLE), F(0x540, 1, T_U8, 1,   SIG_TUNE_SELECTOR),
    F(0x540, 2, T_I16, 0.1f, SIG_FUEL_TANK),   F(0x540, 4, T_I16, 0.1f, SIG_TRANS_TEMP),
    F(0x540, 6, T_I16, 0.1f, SIG_DIFF_TEMP),
    F(0x541, 0, T_I16, 0.1f, SIG_VVT_IN1), F(0x541, 2, T_I16, 0.1f, SIG_VVT_EX1),
    F(0x541, 4, T_I16, 0.1f, SIG_VVT_IN2), F(0x541, 6, T_I16, 0.1f, SIG_VVT_EX2),
    F(0x542, 0, T_I16, 0.1f, SIG_VVT_IN_TARGET), F(0x542, 2, T_I16, 0.1f, SIG_VVT_EX_TARGET),
    F(0x542, 4, T_U16, 1,    SIG_ERROR_CODE),
};
#define N_FIELDS (sizeof(FIELDS) / sizeof(FIELDS[0]))

const uint16_t maxxecu_frame_ids[] = {
    0x520, 0x521, 0x522, 0x523, 0x524, 0x525, 0x526, 0x527, 0x528,
    0x530, 0x531, 0x532, 0x533, 0x534, 0x535, 0x536, 0x537, 0x538, 0x539,
    0x540, 0x541, 0x542, 0
};

static ecu_data_t s_data;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static inline bool is_maxx_id(uint32_t id)
{
    return (id >= 0x520 && id <= 0x528) || (id >= 0x530 && id <= 0x539) ||
           (id >= 0x540 && id <= 0x542);
}

/* --- kohandatud kanalid --- */
static const dash_can_ch_t CUST[] = DASH_CUSTOM_CAN;
#define N_CUST ((int)(sizeof(CUST) / sizeof(CUST[0])))
_Static_assert(sizeof(CUST) / sizeof(CUST[0]) <= 8, "kuni 8 kohandatud CAN-kanalit");

static bool decode_custom(uint32_t can_id, const uint8_t *d, uint8_t dlc)
{
    bool hit = false;
    for (int i = 0; i < N_CUST; i++) {
        const dash_can_ch_t *c = &CUST[i];
        if (!c->id || c->id != can_id) continue;
        int len = c->type >= CAN_U16 ? 2 : 1;
        if (c->byte + len > dlc) continue;
        float raw;
        if (len == 1) {
            raw = c->type == CAN_S8 ? (float)(int8_t)d[c->byte] : (float)d[c->byte];
        } else {
            uint16_t u = c->big_endian ? (uint16_t)(d[c->byte] << 8 | d[c->byte + 1])
                                       : (uint16_t)(d[c->byte] | d[c->byte + 1] << 8);
            raw = c->type == CAN_S16 ? (float)(int16_t)u : (float)u;
        }
        portENTER_CRITICAL(&s_lock);
        s_data.v[SIG_CUST1 + i] = raw * c->mul + c->add;
        s_data.valid[SIG_CUST1 + i] = true;
        portEXIT_CRITICAL(&s_lock);
        hit = true;
    }
    return hit;
}

bool maxxecu_decode(uint32_t can_id, const uint8_t d[8], uint8_t dlc)
{
    diag_can_rx(can_id, dlc);
    bool cust = decode_custom(can_id, d, dlc);
    if (!is_maxx_id(can_id) || dlc < 8) {
        return cust;
    }
    /* Ühes kaadris on kuni 5 välja (0x540) */
    float vals[8];
    uint8_t sigs[8];
    int n = 0;
    for (size_t i = 0; i < N_FIELDS && n < 8; i++) {
        const field_t *f = &FIELDS[i];
        if (f->id != can_id) {
            continue;
        }
        float raw;
        switch (f->type) {
        case T_U8:  raw = d[f->off]; break;
        case T_U16: raw = (uint16_t)(d[f->off] | (d[f->off + 1] << 8)); break;
        default:    raw = (int16_t)(d[f->off] | (d[f->off + 1] << 8)); break;
        }
        vals[n] = raw * f->scale;
        sigs[n] = f->sig;
        n++;
    }
    int64_t now = esp_timer_get_time();

    portENTER_CRITICAL(&s_lock);
    for (int i = 0; i < n; i++) {
        s_data.v[sigs[i]] = vals[i];
        s_data.valid[sigs[i]] = true;
    }
    if (can_id == 0x526) {
        s_data.flags = (uint16_t)(d[0] | (d[1] << 8));
    }
    if (can_id == 0x520) {
        s_data.v[SIG_AFR] = s_data.v[SIG_LAMBDA] * DASH_AFR_STOICH;
        s_data.valid[SIG_AFR] = true;
    }
    if ((can_id == 0x520 || can_id == 0x530) && s_data.valid[SIG_BARO] && s_data.valid[SIG_MAP]) {
        s_data.v[SIG_BOOST_BAR] = (s_data.v[SIG_MAP] - s_data.v[SIG_BARO]) / 100.0f;
        s_data.valid[SIG_BOOST_BAR] = true;
    }
    s_data.last_rx_us = now;
    s_data.frame_count++;
    portEXIT_CRITICAL(&s_lock);
    return true;
}

bool maxxecu_encode(uint32_t can_id, const float *vals, uint16_t flags, uint8_t out[8])
{
    if (!is_maxx_id(can_id)) {
        return false;
    }
    memset(out, 0, 8);
    for (size_t i = 0; i < N_FIELDS; i++) {
        const field_t *f = &FIELDS[i];
        if (f->id != can_id) {
            continue;
        }
        float x = vals[f->sig] / f->scale;
        long r = (long)(x >= 0 ? x + 0.5f : x - 0.5f);
        switch (f->type) {
        case T_U8:
            out[f->off] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
            break;
        case T_U16: {
            uint16_t u = (uint16_t)(r < 0 ? 0 : r > 65535 ? 65535 : r);
            out[f->off] = u & 0xFF; out[f->off + 1] = u >> 8;
            break;
        }
        default: {
            int16_t s = (int16_t)(r < -32768 ? -32768 : r > 32767 ? 32767 : r);
            out[f->off] = (uint16_t)s & 0xFF; out[f->off + 1] = (uint16_t)s >> 8;
            break;
        }
        }
    }
    if (can_id == 0x526) {
        out[0] = flags & 0xFF;
        out[1] = flags >> 8;
    }
    return true;
}

void maxxecu_snapshot(ecu_data_t *out)
{
    portENTER_CRITICAL(&s_lock);
    memcpy(out, &s_data, sizeof(*out));
    portEXIT_CRITICAL(&s_lock);
}
