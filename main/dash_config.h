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
#define DASH_UNIT_TEMP_F      0        /* 1 = temperatuurid °F (muudetav SEADED vaates) */
#define DASH_UNIT_PRESS_PSI   0        /* 1 = rõhud psi */

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
    float       offset;    /* liidetakse pärast korrutamist (vaikimisi 0; ühikute teisendus) */
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
/* Piire saab muuta ka ekraanil (SEADED) ja telefonist (Wi-Fi); siin on vaikeväärtused.
 * full = 1: lisaks ribale suur vilkuv teade üle terve ekraani (ohtlik mootorile). */
typedef struct {
    sig_id_t       sig;
    const char *label;
    float       low;      /* NAN = ei kontrolli */
    float       high;     /* NAN = ei kontrolli */
    float       min_rpm;  /* kontrolli ainult kui pöörded üle selle (0 = alati) */
    uint8_t     full;     /* 1 = täisekraani hoiatus */
} dash_warning_t;

#define DASH_WARNINGS { \
    { SIG_CLT,        "VEE TEMP",    NAN,   105,  0,    1 }, \
    { SIG_OIL_TEMP,   "ÕLI TEMP",    NAN,   135,  0,    0 }, \
    { SIG_OIL_PRESS,  "ÕLI RÕHK",    100,   NAN,  1500, 1 }, \
    { SIG_BATT,       "AKU",         12.0f, NAN,  0,    0 }, \
    { SIG_FUEL_PRESS, "KÜTUSE RÕHK", 200,   NAN,  1500, 1 }, \
    { SIG_EGT_MAX,    "EGT",         NAN,   980,  0,    0 }, \
}

/* Õlirõhu miinimum pöörete järgi (kPa; vahepeal joonelt interpoleeritud, otstes konstantne).
 * Kehtib, kui DASH_OIL_CURVE_ON = 1 – siis ülaltoodud ÕLI RÕHK "low" asendub selle kõveraga.
 * Rusikareegel: ~0,7 bar iga 1000 rpm kohta, aga vaata oma mootori andmeid. */
#define DASH_OIL_CURVE_ON  1
#define DASH_OIL_CURVE_N   4
#define DASH_OIL_CURVE { {800, 70}, {2000, 120}, {4000, 200}, {6000, 250} }
#define DASH_OIL_MIN_RPM   500     /* alla selle (mootor seisab / käivitub) õlirõhku ei kontrollita */

/* ECU olekust tulevad hoiatused (MaxxECU 0x526 lipud ja veakood). full = täisekraan. */
typedef struct {
    uint16_t    flag;     /* FLAG_* (maxxecu.h); 0 = veakood (SIG_ERROR_CODE ≠ 0) */
    const char *label;
    uint8_t     full;
    uint16_t    hold_ms;  /* teadet hoitakse vähemalt nii kaua (lühike KNOCK jääb nähtavaks) */
} dash_ecu_alert_t;

#define DASH_ECU_ALERTS { \
    { FLAG_KNOCK, "KNOCK",    1, 1500 }, \
    { 0,          "ECU VIGA", 0, 0    }, \
}

/* Hoiatuste logi: mitu viimast hoiatust meeles (ka pärast väljalülitamist) */
#define DASH_ALARM_LOG_N   40

/* --- Vaated -------------------------------------------------------------
 * Vaheta: libista ekraanil vasakule/paremale või vajuta lisanuppu
 * (idf.py menuconfig → SimplDash → vaate nupu GPIO). Viimane vaade jäetakse meelde.
 *   0 RACE    – shift lightid, segmenttahhomeeter, 14 näidikut
 *   1 KELLAD  – kaks ümarat kella (pöörded + kiirus), keskel põhinäidud
 *   2 KAAR    – suur kaartahhomeeter, kiirus keskel, 6 näidikut külgedel
 *   3 TÄNAV   – kaardus astmeline tahhomeeter, suur käik sinises kumas, segmentribad
 *   4 KLASSIKA – osutitega kellad: vesi, kütusekulu | spido, tahho | õlirõhk
 *   5 0–100   – kiirendusaeg (ainult GPS-iga)
 *   6 RING    – ringiaja taimer, vahe parimaga, sektorid (ainult GPS-iga)
 *   7 RADA    – raja kaart ja sektorite tabel (ainult GPS-iga)
 *   8 ANDMED  – kõik väärtused koos min/max tippudega
 *   9 SEADED  – ECU nupud (DASH_ECU_BUTTONS), seaded, sõit/läbisõit, hoiatuste logi, info
 * Vaateid saab peita SEADED vaates.
 */
#define DASH_BOOT_MS       2000  /* käivitusekraan logoga (ms), 0 = kohe dash */
#define DASH_PAGE_FADE_MS  300   /* vaate vahetuse üleminek (tumeneb + heleneb), 0 = kohe */
#define DASH_SPEED_MAX  260

/* KLASSIKA: hetke kütusekulu arvutatakse pihustite töötsüklist (MaxxECU ei saada kulu otse):
 * L/h = pihusti cc/min × silindreid × duty% × 60 / 1000;  L/100km = L/h / kiirus × 100 */
#define DASH_INJ_CC_MIN     630     /* ühe pihusti vooluhulk (cc/min, kütuse rõhul) */
#define DASH_CYLINDERS      6       /* RB25DET: 2,5 L, 6 silindrit */
#define DASH_FUEL_GAUGE_MAX 30      /* kütusekulu kella skaala lõpp (L/100km) */
#define DASH_CLASSIC_SPEED_MAX 320  /* KLASSIKA spidomeetri skaala lõpp (km/h) */
#define DASH_CLASSIC_RPM_MAX 11000 /* KLASSIKA tahhomeetri skaala lõpp (rpm), nagu originaalis */
#define DASH_OIL_GAUGE_MAX  8       /* KLASSIKA õlirõhu kella skaala lõpp (bar) */

/* --- GPS ---------------------------------------------------------------
 * Kiiruse allikas: ECU (rattakiirus MaxxECU-st), GPS või AUTO (GPS, kui satelliidid
 * on olemas, muidu ECU). Simulaatoris saab vahetada nupuga.
 */
#define DASH_SPEED_ECU   0
#define DASH_SPEED_GPS   1
#define DASH_SPEED_AUTO  2
#define DASH_SPEED_SOURCE     DASH_SPEED_AUTO
#define DASH_GPS_MIN_KMH      2.0f    /* aeglasem GPS-kiirus = 0 (seistes GPS "hõljub") */
/* Mooduli seadistus käivitusel (vajab GPS TX juhet, menuconfig → SimplDash → GPS TX GPIO):
 * uuendussagedus ja UART kiirus. 10 Hz vajab kiiremat UART-i kui vaikimisi 38400. */
#define DASH_GPS_RATE_HZ      10
#define DASH_GPS_FAST_BAUD    115200

/* Kell (GPS-ist, näidatakse ainult kui GPS on ühendatud): Eesti aeg */
#define DASH_TZ_OFFSET_MIN    120     /* UTC+2 */
#define DASH_TZ_EU_DST        1       /* suveaeg märtsi viimasest pühapäevast oktoobri viimaseni */

/* 0–100 mõõtmine */
#define DASH_ACCEL_TARGET     100     /* km/h */
#define DASH_ACCEL_SPLIT      60      /* vaheaeg km/h */
#define DASH_ACCEL_START_KMH  1.0f    /* liikumise algus */
#define DASH_ACCEL_MAX_KMHS   60.0f   /* suurem kiirendus (km/h sekundis, ~1,7 g) = GPS-i viga, mõõtmine katkeb */

/* RING vaate külgede näidikud (4 + 4) */
#define DASH_LAP_LEFT { \
    { SIG_CLT,        "VESI",        "°C",   0, 1.0f,  40, 120 }, \
    { SIG_OIL_TEMP,   "ÕLI TEMP",    "°C",   0, 1.0f,  40, 150 }, \
    { SIG_OIL_PRESS,  "ÕLI RÕHK",    "bar",  1, 0.01f, 0,  7   }, \
    { SIG_FUEL_PRESS, "KÜTUSE RÕHK", "bar",  1, 0.01f, 0,  6   }, \
}
#define DASH_LAP_RIGHT { \
    { SIG_SPEED,      "KIIRUS",      "km/h", 0, 1.0f,  0,  DASH_SPEED_MAX }, \
    { SIG_BOOST_BAR,  "ÜLERÕHK",     "bar",  2, 1.0f,  -1, 2.5f }, \
    { SIG_AFR,        "AFR",         "",     1, 1.0f,  10, 18  }, \
    { SIG_BATT,       "AKU",         "V",    1, 1.0f,  10, 15  }, \
}

/* Ringiaeg: start/finiš joone laius (m) ja lühim võimalik ring (s) */
#define DASH_LAP_GATE_M       25.0f
#define DASH_LAP_MIN_S        10.0f    /* KELLAD vaate spidomeetri skaala lõpp (km/h) */

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

/* --- Logimine SD-kaardile (TF-pesa) -------------------------------------
 * Iga sõit eraldi CSV-faili /sdcard/logs/ (kuupäev ja kellaaeg GPS-ist, muidu järjekorranumber).
 * Avab Excel, RaceChrono (CSV import) jne. Muudetav ka SEADED vaates. */
#define DASH_LOG_MODE   1      /* 0 väljas, 1 kui mootor töötab (rpm > 300), 2 alati kui ECU andmed tulevad */
#define DASH_LOG_HZ     20     /* ridu sekundis */
#define DASH_LOG_STOP_S 10     /* mootor seisab nii kaua → fail suletakse */
/* Logitavad kanalid (lisaks aeg, GPS ja olekulipud). Nimi = CSV veeru pealkiri. */
#define DASH_LOG_CHANNELS { \
    { SIG_RPM, "RPM" }, { SIG_SPEED, "Speed km/h" }, { SIG_GEAR, "Gear" }, { SIG_TPS, "TPS %" }, \
    { SIG_MAP, "MAP kPa" }, { SIG_BOOST_BAR, "Boost bar" }, { SIG_BOOST_TARGET, "Boost target kPa" }, \
    { SIG_LAMBDA, "Lambda" }, { SIG_AFR, "AFR" }, { SIG_LAMBDA_TARGET, "Lambda target" }, \
    { SIG_IGN_ANGLE, "Ignition deg" }, { SIG_FUEL_DUTY, "Inj duty %" }, { SIG_FUEL_PW, "Inj PW ms" }, \
    { SIG_CLT, "CLT C" }, { SIG_IAT, "IAT C" }, { SIG_OIL_TEMP, "Oil temp C" }, \
    { SIG_OIL_PRESS, "Oil press kPa" }, { SIG_FUEL_PRESS, "Fuel press kPa" }, { SIG_BATT, "Battery V" }, \
    { SIG_EGT_MAX, "EGT max C" }, { SIG_KNOCK_COUNT, "Knock count" }, { SIG_KNOCK_CORR, "Knock corr deg" }, \
    { SIG_ETHANOL, "Ethanol %" }, { SIG_FUEL_TANK, "Fuel tank L" }, { SIG_WHEEL_SLIP, "Wheel slip %" }, \
    { SIG_ACC_FWD, "Acc fwd g" }, { SIG_ACC_RIGHT, "Acc right g" }, \
}

/* --- Wi-Fi (plaadi ESP32-C6) ---------------------------------------------
 * Dash teeb oma Wi-Fi võrgu; telefoniga sellega ühendudes ava http://192.168.4.1
 * (seaded, logifailid, hoiatuste logi, püsivara uuendus). */
#define DASH_WIFI_ON    1
#define DASH_WIFI_SSID  "SimplDash"
#define DASH_WIFI_PASS  "simpldash"   /* min 8 märki; muuda SEADED vaates või veebilehel */

/* --- Läbisõit, mootoritunnid, reis --------------------------------------- */
#define DASH_ENGINE_RUN_RPM  400     /* üle selle loetakse mootoritunde */
#define DASH_STATS_SAVE_S    60      /* kui tihti läbisõit/tunnid püsimällu kirjutatakse */

/* --- MaxxECU: kohandatud CAN-kaadrid (vastuvõtt) -------------------------
 * Kanalid, mida MaxxECU Default protokoll ei saada: seadista MTune-is CAN output (CAN Bus →
 * CAN outputs, "User defined") ja kirjelda siin sama kaader. Kuni 8 kanalit → SIG_CUST1..8,
 * mida saab kasutada kõigis näidikutes (nt { SIG_CUST1, "TC TASE", "", 0, 1.0f, 0, 9 }).
 * Väli: CAN ID (hex), bait (0–7), tüüp, big-endian?, korrutaja, liidetav, nimi, ühik.
 * Väärtus = toorväärtus × korrutaja + liidetav. id = 0 → kanal pole kasutusel. */
#define CAN_U8   0
#define CAN_S8   1
#define CAN_U16  2
#define CAN_S16  3
typedef struct {
    uint16_t    id;
    uint8_t     byte;
    uint8_t     type;
    uint8_t     big_endian;
    float       mul, add;
    const char *name, *unit;
} dash_can_ch_t;

#define DASH_CUSTOM_CAN { \
    /* näide: MaxxECU saadab 0x600 baidis 0 veojõukontrolli astme (8 bit) */ \
    /* { 0x600, 0, CAN_U8,  0, 1.0f, 0, "TC TASE", "" }, */ \
    /* { 0x600, 2, CAN_S16, 0, 0.1f, 0, "SIHTRÕHK", "kPa" }, */ \
    { 0 } \
}

/* --- Dash → MaxxECU: nupud (SEADED vaade → ECU vahekaart) --------------------
 * Dash saadab DASH_CAN_TX_HZ korda sekundis ühe kaadri (ID DASH_CAN_TX_ID, 8 baiti):
 *   bait 0 = lülitite bitid (bit 0..7), baidid 1..6 = valikute väärtused, bait 7 = loendur.
 * MTune-is: CAN Bus → CAN inputs → "CAN message input", ID = DASH_CAN_TX_ID (kümnendina),
 *   lüliti: ByteOffset 0, Type 8 bit, "bit mask" = 1 << bit, mode "Momentary", input function = soovitud funktsioon;
 *   valik:  ByteOffset = bait, Type 8 bit, väärtus → nt kaardivalik / veojõu aste (User CAN input).
 *   Lülita sisse "timeout" (nt 0,5 s), et dashi kadumisel lülitid nulli läheksid.
 * Saatmine on vaikimisi VÄLJAS – lülita sisse SEADED vaates (ECU JUHTIMINE), kui MTune on seadistatud.
 * NB: listen-only režiimis (menuconfig) dash saata ei saa. */
#define DASH_CAN_TX_ON   0
#define DASH_CAN_TX_ID   0x5F0
#define DASH_CAN_TX_HZ   10

#define BTN_TOGGLE   0   /* vajutus lülitab bit-i sisse/välja (jäetakse meelde) */
#define BTN_HOLD     1   /* bit on 1 ainult vajutuse ajal */
#define BTN_SELECT   2   /* väärtus 1..max baidis; vajutus = järgmine (max → 1) */
typedef struct {
    const char *label;
    uint8_t     kind;
    uint8_t     byte;    /* SELECT: bait 1..6; TOGGLE/HOLD: alati bait 0 */
    uint8_t     bit;     /* TOGGLE/HOLD: bit 0..7 */
    uint8_t     max;     /* SELECT: suurim väärtus */
} dash_ecu_btn_t;

#define DASH_ECU_BUTTONS { \
    { "LAUNCH",     BTN_TOGGLE, 0, 0, 0 }, \
    { "ANTI-LAG",   BTN_TOGGLE, 0, 1, 0 }, \
    { "TÜHIK. TÕUS",BTN_HOLD,   0, 3, 0 }, /* tühikäigu tõstmine (MaxxECU "AC request / idle up") */ \
    { "SEADE",      BTN_SELECT, 1, 0, 4 }, /* ECU seadistus (tune) 1–4 */ \
    { "BOOST",      BTN_SELECT, 3, 0, 4 }, /* ülerõhu seade 1–4 */ \
    { "VEOJÕUD",    BTN_SELECT, 2, 0, 9 }, \
}
