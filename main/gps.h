/*
 * GPS (NMEA 0183) – nt Ebyte E108-GN04G-TTL UART-i kaudu.
 * Parser on platvormist sõltumatu (töötab ka simulaatoris); UART ainult plaadil.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool     connected;     /* NMEA lauseid on tulnud viimase GPS_TIMEOUT_MS jooksul */
    bool     fix;           /* asukoht ja kiirus kehtivad */
    float    speed_kmh;
    float    course;        /* sõidusuund, kraadid (0 = põhi, päripäeva) */
    double   lat, lon;      /* kraadid */
    int      sats;
    float    hdop;
    bool     time_valid;    /* kuupäev ja kellaaeg (UTC) kehtivad */
    int      year, month, day, hour, min;
    float    sec;
    int64_t  sample_us;     /* esp_timer aeg, millal viimane RMC saabus */
    uint32_t samples;       /* RMC lausete loendur (uus mõõtmine = muutus) */
    int64_t  last_rx_us;
} gps_data_t;

#define GPS_TIMEOUT_MS 3000

/* Anna parserile baite (UART-ist või simulaatorist). Lõimekindel. */
void gps_feed(const char *data, size_t len);
/* Viimane olek (connected arvutatakse praeguse aja järgi) */
void gps_snapshot(gps_data_t *out);
/* Unusta kõik (nt simulaatoris GPS lahti ühendatud) */
void gps_reset(void);
/* Plaadil: käivita UART (Kconfig SIMPLDASH_GPS_*). Simulaatoris ei tee midagi. */
void gps_start(void);
