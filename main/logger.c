#include "logger.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "settings.h"

typedef struct { sig_id_t sig; const char *name; } log_ch_t;
static const log_ch_t CH[] = DASH_LOG_CHANNELS;
#define N_CH (sizeof(CH) / sizeof(CH[0]))

static struct {
    bool     card, active, error;
    char     root[64], dir[72];
    char     file[32];
    uint32_t rows, dropped;
    int64_t  next_us, t0_us, idle_since;
    int      lap;
    float    lap_time;
    bool     lap_pending;
    FILE    *f;
} lg;

/* --- kirjutamine: plaadil eraldi task + puhver, simulaatoris otse ------------------ */
#ifndef DASH_HOST
#include "esp_board_manager.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
static const char *TAG = "log";
static StreamBufferHandle_t s_sb;
static volatile int s_cmd;          /* 1 = ava lg.file, 2 = sulge */
#define SB_SIZE (48 * 1024)
#endif

static bool mkdirs(void)
{
    struct stat st;
    if (stat(lg.dir, &st) == 0) return true;
    return mkdir(lg.dir, 0775) == 0;
}

/* järgmine vaba LOGnnnnn.csv number */
static int next_num(void)
{
    int mx = 0;
    DIR *d = opendir(lg.dir);
    if (!d) return 1;
    struct dirent *e;
    while ((e = readdir(d))) {
        int n;
        if (sscanf(e->d_name, "LOG%5d", &n) == 1 && n > mx) mx = n;
    }
    closedir(d);
    return mx + 1;
}

static void write_header(FILE *f)
{
    fprintf(f, "Time s,UTC,Lat,Lon,GPS speed km/h,Heading deg,Sats");
    for (size_t i = 0; i < N_CH; i++) fprintf(f, ",%s", CH[i].name);
    fprintf(f, ",Flags,Alarms,Lap,Lap time s\n");
}

static void file_open(void)
{
    char p[128];
    snprintf(p, sizeof(p), "%s/%s", lg.dir, lg.file);
    lg.f = fopen(p, "w");
    if (!lg.f) {
        lg.error = true;
        return;
    }
    setvbuf(lg.f, NULL, _IOFBF, 16 * 1024);
    write_header(lg.f);
}

static void file_close(void)
{
    if (lg.f) {
        fclose(lg.f);
        lg.f = NULL;
    }
}

#ifndef DASH_HOST
static void writer_task(void *arg)
{
    (void)arg;
    static char buf[4096];
    int64_t last_sync = 0;
    for (;;) {
        if (!lg.card) {
            /* kaart puudub – proovi iga 10 s uuesti (kaardi võib hiljem sisse panna) */
            if (esp_board_manager_init_device_by_name("fs_sdcard") == ESP_OK) {
                lg.card = mkdirs();
                ESP_LOGI(TAG, "SD-kaart %s", lg.card ? "olemas" : "– logikausta ei saa luua");
            }
            if (!lg.card) {
                vTaskDelay(pdMS_TO_TICKS(10000));
                continue;
            }
        }
        if (s_cmd == 1) {
            file_close();
            file_open();
            s_cmd = 0;
        }
        size_t n = xStreamBufferReceive(s_sb, buf, sizeof(buf), pdMS_TO_TICKS(200));
        if (n && lg.f) {
            if (fwrite(buf, 1, n, lg.f) != n) lg.error = true;
        }
        int64_t now = esp_timer_get_time();
        if (lg.f && now - last_sync > 2000000) {     /* iga 2 s kettale (voolu katkemisel kaob max 2 s) */
            fflush(lg.f);
            fsync(fileno(lg.f));
            last_sync = now;
        }
        if (s_cmd == 2 && xStreamBufferIsEmpty(s_sb)) {
            file_close();
            s_cmd = 0;
        }
    }
}
#endif

void logger_start(void)
{
#ifndef DASH_HOST
    snprintf(lg.root, sizeof(lg.root), "/sdcard");
    snprintf(lg.dir, sizeof(lg.dir), "%s/logs", lg.root);
    s_sb = xStreamBufferCreate(SB_SIZE, 1);
    xTaskCreatePinnedToCore(writer_task, "logger", 6144, NULL, 3, NULL, 0);
#else
    const char *d = getenv("SIMPLDASH_DATA");
    if (!d || !*d) return;
    snprintf(lg.root, sizeof(lg.root), "%s/sdcard", d);
    mkdir(lg.root, 0775);
    snprintf(lg.dir, sizeof(lg.dir), "%s/logs", lg.root);
    lg.card = mkdirs();
#endif
}

#ifndef DASH_HOST
#include "esp_vfs_fat.h"
bool logger_space(uint64_t *total, uint64_t *free_b)
{
    if (!lg.card) return false;
    return esp_vfs_fat_info(lg.root, total, free_b) == ESP_OK;
}
static int64_t now_us(void) { return esp_timer_get_time(); }
#else
#include <sys/statvfs.h>
#include <time.h>
bool logger_space(uint64_t *total, uint64_t *free_b)
{
    struct statvfs v;
    if (!lg.card || statvfs(lg.root, &v) != 0) return false;
    *total = (uint64_t)v.f_blocks * v.f_frsize;
    *free_b = (uint64_t)v.f_bavail * v.f_frsize;
    return true;
}
static int64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#endif

float logger_speed_test(void)
{
    if (!lg.card) return -1;
    char p[128];
    snprintf(p, sizeof(p), "%s/_speedtest.bin", lg.dir);
    static char buf[4096];
    memset(buf, 0x55, sizeof(buf));
    int64_t t0 = now_us();
    FILE *f = fopen(p, "wb");
    if (!f) return -1;
    bool ok = true;
    for (int i = 0; i < 64 && ok; i++) ok = fwrite(buf, 1, sizeof(buf), f) == sizeof(buf);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    int64_t dt = now_us() - t0;
    remove(p);
    if (!ok || dt <= 0) return -1;
    return 256.0f * 1e6f / (float)dt;
}

const char *logger_dir(void)
{
    return lg.card ? lg.dir : NULL;
}

void logger_status(logger_status_t *o)
{
    o->card = lg.card;
    o->active = lg.active;
    snprintf(o->file, sizeof(o->file), "%s", lg.file);
    o->rows = lg.rows;
    o->dropped = lg.dropped;
    o->error = lg.error;
}

void logger_mark_lap(int lap, float t)
{
    lg.lap = lap;
    lg.lap_time = t;
    lg.lap_pending = true;
}

static void emit(const char *row, size_t n)
{
#ifndef DASH_HOST
    if (xStreamBufferSpacesAvailable(s_sb) < n) {
        lg.dropped++;
        return;
    }
    xStreamBufferSend(s_sb, row, n, 0);
#else
    if (lg.f) fwrite(row, 1, n, lg.f);
#endif
    lg.rows++;
}

static void start_session(const dash_state_t *s)
{
    if (s->gps.time_valid) {
        snprintf(lg.file, sizeof(lg.file), "%04d%02d%02d_%02d%02d%02d.csv", s->gps.year, s->gps.month, s->gps.day,
                 s->gps.hour, s->gps.min, (int)s->gps.sec);
    } else {
        snprintf(lg.file, sizeof(lg.file), "LOG%05d.csv", next_num());
    }
    lg.t0_us = s->now_us;
    lg.rows = 0;
    lg.dropped = 0;
    lg.error = false;
    lg.active = true;
#ifndef DASH_HOST
    s_cmd = 1;
#else
    file_open();
#endif
}

static void stop_session(void)
{
    lg.active = false;
#ifndef DASH_HOST
    s_cmd = 2;
#else
    file_close();
#endif
}

void logger_update(const dash_state_t *s)
{
    if (!lg.card) return;
    bool want = false;
    if (g_set.log_mode == 1) want = !s->stale && s->rpm > 300;
    else if (g_set.log_mode == 2) want = !s->stale;

    if (want) lg.idle_since = 0;
    if (lg.active && !want) {
        if (!lg.idle_since) lg.idle_since = s->now_us;
        if (g_set.log_mode == 0 || s->now_us - lg.idle_since > (int64_t)DASH_LOG_STOP_S * 1000000) stop_session();
    }
    if (!lg.active && want) start_session(s);
    if (!lg.active || s->now_us < lg.next_us) return;

    const int hz = g_set.log_hz > 0 ? g_set.log_hz : 10;
    const int64_t period = 1000000 / hz;
    lg.next_us = (lg.next_us && s->now_us - lg.next_us < period) ? lg.next_us + period : s->now_us + period;

    static char row[1024];
    int n = snprintf(row, sizeof(row), "%.3f,", (s->now_us - lg.t0_us) / 1e6);
    const gps_data_t *g = &s->gps;
    if (g->time_valid)
        n += snprintf(row + n, sizeof(row) - n, "%02d:%02d:%06.3f", g->hour, g->min, g->sec);
    if (g->fix)
        n += snprintf(row + n, sizeof(row) - n, ",%.7f,%.7f,%.2f,%.1f,%d", g->lat, g->lon, g->speed_kmh, g->course,
                      g->sats);
    else
        n += snprintf(row + n, sizeof(row) - n, ",,,,,%d", g->sats);
    for (size_t i = 0; i < N_CH && n < (int)sizeof(row) - 64; i++) {
        if (s->d.valid[CH[i].sig]) n += snprintf(row + n, sizeof(row) - n, ",%.4g", s->d.v[CH[i].sig]);
        else n += snprintf(row + n, sizeof(row) - n, ",");
    }
    n += snprintf(row + n, sizeof(row) - n, ",%u,", s->d.flags);
    /* aktiivsed hoiatused tekstina (koma asemel /) */
    for (const char *c = s->warn_txt; *c && n < (int)sizeof(row) - 40; c++) {
        if (*c != ',') row[n++] = *c;
    }
    if (lg.lap_pending) {
        n += snprintf(row + n, sizeof(row) - n, ",%d,%.3f\n", lg.lap, lg.lap_time);
        lg.lap_pending = false;
    } else {
        n += snprintf(row + n, sizeof(row) - n, ",,\n");
    }
    emit(row, (size_t)n);
}
