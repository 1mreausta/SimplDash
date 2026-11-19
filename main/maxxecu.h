/*
 * MaxxECU "Default" CAN-väljundprotokoll (v1.2 / v1.3)
 * Allikas: https://maxxecu.com/webhelp/can-default_maxxecu_protocol.html
 * 500 kbit/s, 11-bit ID, little-endian, int16 signed (kui pole märgitud teisiti).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Kõik signaalid, mida dekooder välja annab. Lisa uusi siia + tabelisse maxxecu.c-s. */
typedef enum {
    SIG_RPM, SIG_TPS, SIG_MAP, SIG_LAMBDA,
    SIG_LAMBDA_A, SIG_LAMBDA_B, SIG_IGN_ANGLE, SIG_IGN_CUT,
    SIG_FUEL_PW, SIG_FUEL_DUTY, SIG_FUEL_CUT, SIG_SPEED,
    SIG_UNDRIVEN_SPEED, SIG_DRIVEN_SPEED, SIG_WHEEL_SLIP, SIG_TARGET_SLIP,
    SIG_TC_POWER_LIMIT, SIG_LAMBDA_CORR_A, SIG_LAMBDA_CORR_B, SIG_FW_VERSION,
    SIG_USER_CH9, SIG_USER_CH10, SIG_USER_CH11, SIG_USER_CH12,
    SIG_REVLIMIT_RPM,
    SIG_ACC_FWD, SIG_ACC_RIGHT, SIG_ACC_UP, SIG_LAMBDA_TARGET,
    SIG_KNOCK_PEAK, SIG_KNOCK_CORR, SIG_KNOCK_COUNT, SIG_KNOCK_LAST_CYL,
    SIG_BATT, SIG_BARO, SIG_IAT, SIG_CLT,
    SIG_FUEL_TRIM, SIG_ETHANOL, SIG_IGN_COMP, SIG_EGT1,
    SIG_EGT2, SIG_EGT3, SIG_EGT4, SIG_EGT5,
    SIG_EGT6, SIG_EGT7, SIG_EGT8, SIG_EGT_MAX,
    SIG_EGT_DIFF, SIG_ECU_TEMP, SIG_ERROR_COUNT, SIG_LOST_SYNC,
    SIG_AIN1, SIG_AIN2, SIG_AIN3, SIG_AIN4,
    SIG_GEAR, SIG_BOOST_DUTY, SIG_OIL_PRESS, SIG_OIL_TEMP,
    SIG_FUEL_PRESS, SIG_WG_PRESS, SIG_CLT_PRESS, SIG_BOOST_TARGET,
    SIG_USER_CH1, SIG_USER_CH2, SIG_USER_CH3, SIG_USER_CH4,
    SIG_USER_CH5, SIG_USER_CH6, SIG_USER_CH7, SIG_USER_CH8,
    SIG_BOOST_TABLE, SIG_TUNE_SELECTOR, SIG_FUEL_TANK, SIG_TRANS_TEMP, SIG_DIFF_TEMP,
    SIG_VVT_IN1, SIG_VVT_EX1, SIG_VVT_IN2, SIG_VVT_EX2,
    SIG_VVT_IN_TARGET, SIG_VVT_EX_TARGET, SIG_ERROR_CODE,
    /* Tuletatud (arvutatakse dekoodris) */
    SIG_AFR,        /* lambda * stöhh. suhe */
    SIG_BOOST_BAR,  /* (MAP - baro) / 100 */
    /* Kohandatud CAN-kanalid (DASH_CUSTOM_CAN, dash_config.h) */
    SIG_CUST1, SIG_CUST2, SIG_CUST3, SIG_CUST4, SIG_CUST5, SIG_CUST6, SIG_CUST7, SIG_CUST8,
    SIG_COUNT
} sig_id_t;

/* Olekulipud kaadrist 0x526 (bitimask) */
enum {
    FLAG_SHIFTCUT   = 1u << 0,  FLAG_REVLIMIT   = 1u << 1,
    FLAG_ANTILAG    = 1u << 2,  FLAG_LAUNCH     = 1u << 3,
    FLAG_TC         = 1u << 4,  FLAG_BLIP       = 1u << 5,
    FLAG_AC_IDLE    = 1u << 6,  FLAG_KNOCK      = 1u << 7,
    FLAG_BRAKE      = 1u << 8,  FLAG_CLUTCH     = 1u << 9,
    FLAG_SPEEDLIMIT = 1u << 10, FLAG_GPLIMIT    = 1u << 11,
    FLAG_USERCUT    = 1u << 12, FLAG_LOGGING    = 1u << 13,
    FLAG_NITROUS    = 1u << 14,
};

typedef struct {
    float    v[SIG_COUNT];
    bool     valid[SIG_COUNT];   /* kas signaal on kunagi saabunud */
    uint16_t flags;
    int64_t  last_rx_us;         /* viimase MaxxECU kaadri aeg (esp_timer) */
    uint32_t frame_count;
} ecu_data_t;

/* Dekodeeri kaader jagatud olekusse (lõimekindel). Tagastab true, kui ID oli MaxxECU oma. */
bool maxxecu_decode(uint32_t can_id, const uint8_t data[8], uint8_t dlc);

/* Kodeeri väärtused kaadriks (demorežiim). vals[] indekseeritud sig_id_t järgi. */
bool maxxecu_encode(uint32_t can_id, const float *vals, uint16_t flags, uint8_t out[8]);

/* Kõik MaxxECU kaadri-ID-d (demorežiimi jaoks), lõpetatud 0-ga */
extern const uint16_t maxxecu_frame_ids[];

/* Kopeeri hetkeseis (lõimekindel) */
void maxxecu_snapshot(ecu_data_t *out);

#ifdef __cplusplus
}
#endif
