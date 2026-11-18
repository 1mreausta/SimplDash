/*
 * SimplDash seaded – kõik piirid, näidikud ja shift-lightid on siin.
 * Muuda ja laadi püsivara uuesti (idf.py flash).
 */
#pragma once
#include <math.h>
#include "maxxecu.h"

/* --- Üldine ------------------------------------------------------------- */
#define DASH_RPM_MAX          9000     /* tahhomeetri skaala lõpp */
#define DASH_SHIFT_START      6200     /* esimene shift-light süttib */
#define DASH_SHIFT_FLASH      7500     /* kõik vilguvad siniselt = vaheta käiku */
#define DASH_AFR_STOICH       14.7f    /* bensiin 14.7, E85 ~9.8 */
#define DASH_COMM_TIMEOUT_MS  1000     /* kui kaua võib ECU vaikida, enne kui hoiatus tuleb */
#define DASH_UI_PERIOD_MS     33       /* ~30 Hz värskendus */

/* Ekraani heleduse astmed (%) – ekraani puudutus vahetab */
#define DASH_BRIGHTNESS_LEVELS { 100, 50, 20 }

/* --- Näidikud ----------------------------------------------------------- */
typedef struct {
    sig_id_t       sig;
    const char *label;
    const char *unit;
    uint8_t     decimals;
    float       scale;     /* kuvamisel korrutatakse (nt kPa -> bar = 0.01) */
    float       bar_min;   /* riba skaala (kui bar_min == bar_max, riba pole) */
    float       bar_max;
} dash_tile_t;

/* Vasak tulp (4 tk) */
#define DASH_TILES_LEFT { \
    { SIG_CLT,       "VESI",      "°C",  0, 1.0f,  40, 120 }, \
    { SIG_OIL_TEMP,  "ÕLI TEMP",  "°C",  0, 1.0f,  40, 150 }, \
    { SIG_OIL_PRESS, "ÕLI RÕHK",  "bar", 1, 0.01f, 0,  7   }, \
    { SIG_IAT,       "SISSELASE", "°C",  0, 1.0f,  0,  80  }, \
}

/* Parem tulp (4 tk) */
#define DASH_TILES_RIGHT { \
    { SIG_AFR,        "AFR",     "",    1, 1.0f,  10, 18  }, \
    { SIG_BOOST_BAR,  "ÜLERÕHK", "bar", 2, 1.0f,  -1, 2.5f }, \
    { SIG_FUEL_PRESS, "KÜTUS",   "bar", 1, 0.01f, 0,  6   }, \
    { SIG_BATT,       "AKU",     "V",   1, 1.0f,  10, 15  }, \
}

/* Alumine rida (6 tk) */
#define DASH_TILES_BOTTOM { \
    { SIG_EGT_MAX,   "EGT MAX",    "°C", 0, 1.0f, 0, 0 }, \
    { SIG_IGN_ANGLE, "SÜÜDE",      "°",  1, 1.0f, 0, 0 }, \
    { SIG_FUEL_DUTY, "INJ. DUTY",  "%",  0, 1.0f, 0, 0 }, \
    { SIG_TPS,       "TPS",        "%",  0, 1.0f, 0, 0 }, \
    { SIG_FUEL_TANK, "KÜTUSEPAAK", "L",  1, 1.0f, 0, 0 }, \
    { SIG_ETHANOL,   "ETANOOL",    "%",  0, 1.0f, 0, 0 }, \
}

/* --- Hoiatused ---------------------------------------------------------- */
typedef struct {
    sig_id_t       sig;
    const char *label;
    float       low;      /* NAN = ei kontrolli */
    float       high;     /* NAN = ei kontrolli */
    float       min_rpm;  /* kontrolli ainult kui pöörded üle selle (0 = alati) */
} dash_warning_t;

#define DASH_WARNINGS { \
    { SIG_CLT,        "VEE TEMP",    NAN,   105,  0    }, \
    { SIG_OIL_TEMP,   "ÕLI TEMP",    NAN,   135,  0    }, \
    { SIG_OIL_PRESS,  "ÕLI RÕHK",    100,   NAN,  1500 }, \
    { SIG_BATT,       "AKU",         12.0f, NAN,  0    }, \
    { SIG_FUEL_PRESS, "KÜTUSE RÕHK", 250,   NAN,  1500 }, \
    { SIG_EGT_MAX,    "EGT",         NAN,   980,  0    }, \
}

/* --- Vaated -------------------------------------------------------------
 * Vaheta: libista ekraanil vasakule/paremale või vajuta lisanuppu
 * (idf.py menuconfig → SimplDash → vaate nupu GPIO). Viimane vaade jäetakse meelde.
 *   0 RACE    – shift lightid, segmenttahhomeeter, 14 näidikut
 *   1 KELLAD  – kaks ümarat kella (pöörded + kiirus), keskel põhinäidud
 *   2 KAAR    – suur kaartahhomeeter, kiirus keskel, 6 näidikut külgedel
 *   3 TÄNAV   – kaardus astmeline tahhomeeter, suur käik sinises kumas, segmentribad
 *   4 ANDMED  – kõik väärtused koos min/max tippudega
 */
#define DASH_PAGE_FADE_MS  300   /* vaate vahetuse üleminek (tumeneb + heleneb), 0 = kohe */
#define DASH_SPEED_MAX  260    /* KELLAD vaate spidomeetri skaala lõpp (km/h) */

/* KELLAD: keskmine tulp (5 tk) */
#define DASH_GAUGES_CENTER { \
    { SIG_OIL_PRESS,  "ÕLI",   "bar", 1, 0.01f, 0, 7 }, \
    { SIG_FUEL_PRESS, "KÜTUS", "bar", 1, 0.01f, 0, 6 }, \
    { SIG_AFR,        "AFR",   "",    1, 1.0f, 10, 18 }, \
    { SIG_BATT,       "AKU",   "V",   1, 1.0f, 10, 15 }, \
    { SIG_IAT,        "IAT",   "°C",  0, 1.0f,  0, 80 }, \
}
/* KELLAD: vertikaalsed ribad kellade kõrval (vasak, parem) */
#define DASH_GAUGES_BAR_LEFT   { SIG_CLT,       "VESI",  "°C", 0, 1.0f, 40, 120 }
#define DASH_GAUGES_BAR_RIGHT  { SIG_FUEL_TANK, "PAAK",  "L",  0, 1.0f, 0,  60  }
/* KELLAD: alumine rida (4 tk) */
#define DASH_GAUGES_BOTTOM { \
    { SIG_OIL_TEMP,  "ÕLI TEMP",  "°C", 0, 1.0f, 0, 0 }, \
    { SIG_EGT_MAX,   "EGT MAX",   "°C", 0, 1.0f, 0, 0 }, \
    { SIG_IGN_ANGLE, "SÜÜDE",     "°",  1, 1.0f, 0, 0 }, \
    { SIG_FUEL_DUTY, "INJ. DUTY", "%",  0, 1.0f, 0, 0 }, \
}

/* KAAR: külgede näidikud (3 + 3) */
#define DASH_SWEEP_LEFT { \
    { SIG_OIL_PRESS, "ÕLI RÕHK", "bar", 1, 0.01f, 0, 7 }, \
    { SIG_OIL_TEMP,  "ÕLI TEMP", "°C",  0, 1.0f, 40, 150 }, \
    { SIG_BATT,      "AKU",      "V",   1, 1.0f, 10, 15 }, \
}
#define DASH_SWEEP_RIGHT { \
    { SIG_BOOST_BAR, "ÜLERÕHK", "bar", 2, 1.0f, -1, 2.5f }, \
    { SIG_AFR,       "AFR",     "",    1, 1.0f, 10, 18 }, \
    { SIG_CLT,       "VESI",    "°C",  0, 1.0f, 40, 120 }, \
}

/* TÄNAV: keskmine ruudustik (2 rida × 3) */
#define DASH_STREET_GRID { \
    { SIG_BATT,       "AKU",         "V",   1, 1.0f,  0, 0 }, \
    { SIG_IAT,        "SISSELASE",   "°C",  0, 1.0f,  0, 0 }, \
    { SIG_FUEL_PRESS, "KÜTUSE RÕHK", "bar", 1, 0.01f, 0, 0 }, \
    { SIG_AFR,        "AFR",         "",    1, 1.0f,  0, 0 }, \
    { SIG_OIL_PRESS,  "ÕLI RÕHK",    "bar", 1, 0.01f, 0, 0 }, \
    { SIG_TPS,        "TPS",         "%",   0, 1.0f,  0, 0 }, \
}
/* TÄNAV: segmentribad (vasakul 2, paremal 1). bar_min/bar_max = skaala */
#define DASH_STREET_STACK_L1 { SIG_CLT,       "VESI", "°C",  0, 1.0f, 40, 120 }
#define DASH_STREET_STACK_L2 { SIG_OIL_TEMP,  "ÕLI",  "°C",  0, 1.0f, 40, 150 }
#define DASH_STREET_STACK_R  { SIG_BOOST_BAR, "ÜLERÕHK", "bar", 2, 1.0f, -1, 2 }

/* ANDMED: tabeli read (2 tulpa × 12). bar_min/bar_max ei kasutata. */
#define DASH_DATA_ROWS { \
    { SIG_RPM,         "PÖÖRDED",     "rpm", 0, 1.0f,   0, 0 }, \
    { SIG_SPEED,       "KIIRUS",      "km/h",0, 1.0f,   0, 0 }, \
    { SIG_BOOST_BAR,   "ÜLERÕHK",     "bar", 2, 1.0f,   0, 0 }, \
    { SIG_MAP,         "MAP",         "kPa", 0, 1.0f,   0, 0 }, \
    { SIG_AFR,         "AFR",         "",    1, 1.0f,   0, 0 }, \
    { SIG_LAMBDA,      "LAMBDA",      "",   2, 1.0f,   0, 0 }, \
    { SIG_TPS,         "TPS",         "%",   0, 1.0f,   0, 0 }, \
    { SIG_IGN_ANGLE,   "SÜÜDE",       "°",   1, 1.0f,   0, 0 }, \
    { SIG_FUEL_DUTY,   "INJ. DUTY",   "%",   0, 1.0f,   0, 0 }, \
    { SIG_FUEL_PW,     "INJ. AEG",    "ms",  2, 1.0f,   0, 0 }, \
    { SIG_ETHANOL,     "ETANOOL",     "%",   0, 1.0f,   0, 0 }, \
    { SIG_KNOCK_COUNT, "KNOCK",       "",    0, 1.0f,   0, 0 }, \
    { SIG_CLT,         "VESI",        "°C",  0, 1.0f,   0, 0 }, \
    { SIG_OIL_TEMP,    "ÕLI TEMP",    "°C",  0, 1.0f,   0, 0 }, \
    { SIG_OIL_PRESS,   "ÕLI RÕHK",    "bar", 1, 0.01f,  0, 0 }, \
    { SIG_FUEL_PRESS,  "KÜTUSE RÕHK", "bar", 1, 0.01f,  0, 0 }, \
    { SIG_IAT,         "SISSELASE",   "°C",  0, 1.0f,   0, 0 }, \
    { SIG_BATT,        "AKU",         "V",   1, 1.0f,   0, 0 }, \
    { SIG_EGT_MAX,     "EGT MAX",     "°C",  0, 1.0f,   0, 0 }, \
    { SIG_EGT_DIFF,    "EGT VAHE",    "°C",  0, 1.0f,   0, 0 }, \
    { SIG_ECU_TEMP,    "ECU TEMP",    "°C",  0, 1.0f,   0, 0 }, \
    { SIG_FUEL_TANK,   "KÜTUSEPAAK",  "L",   1, 1.0f,   0, 0 }, \
    { SIG_LOST_SYNC,   "SÜNK. KADU",  "",    0, 1.0f,   0, 0 }, \
    { SIG_ERROR_CODE,  "VEAKOOD",     "",    0, 1.0f,   0, 0 }, \
}
