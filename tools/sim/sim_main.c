/*
 * SimplDash simulaator arvutis.
 * Käivitab sama kasutajaliidese koodi (dash_ui.c, page_*.c, maxxecu.c, demo.c) ja näitab
 * ekraani brauseris aadressil http://localhost:8090. Hiir = puuteekraan.
 * Ei vaja lisateeke – ainult clang ja brauser.
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "dash_ui.h"
#include "demo.h"
#include "lamps.h"
#include "settings.h"
#include "lvgl.h"
#include "maxxecu.h"
#include "logger.h"
#include "web_api.h"

#define SW 1024
#define SH 600

int64_t g_fake_time_us;  /* esp_timer_get_time() stubi allikas */

static uint16_t s_fb[SW * SH];        /* viimane valmis kaader (RGB565) */
static uint32_t s_frame_no = 1;
static int s_bright = 100, s_page = 0;
static bool s_can = true;             /* kas "ECU" saadab andmeid */
/* hiire sündmuste järjekord – et kiire klõps (vajutus+vabastus) ei kaoks LVGL-i lugemiste vahele */
typedef struct { int16_t x, y; bool pressed; } ev_t;
static ev_t s_q[64];
static int s_qh, s_qt;
static ev_t s_last;

static int64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static uint32_t tick_cb(void) { return (uint32_t)(g_fake_time_us / 1000); }

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a; (void)px;
    if (lv_display_flush_is_last(d)) {
        memcpy(s_fb, lv_display_get_buf_active(d)->data, sizeof(s_fb));
        s_frame_no++;
    }
    lv_display_flush_ready(d);
}

static void touch_cb(lv_indev_t *i, lv_indev_data_t *data)
{
    (void)i;
    if (s_qh != s_qt) {
        s_last = s_q[s_qt];
        s_qt = (s_qt + 1) % 64;
        data->continue_reading = s_qh != s_qt;
    }
    data->point.x = s_last.x;
    data->point.y = s_last.y;
    data->state = s_last.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void on_bright(uint8_t pct) { s_bright = pct; }
static void on_page(int p) { s_page = p; }

/* ------------------------------------------------------------------ HTTP */
extern const char sim_html[];

static void send_all(int fd, const void *buf, size_t n)
{
    const char *p = buf;
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) {
            if (w < 0 && errno == EINTR) continue;
            return;
        }
        p += w;
        n -= (size_t)w;
    }
}

static void reply(int fd, const char *status, const char *type, const char *extra, const void *body, size_t n)
{
    char h[512];
    int hl = snprintf(h, sizeof(h),
                      "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                      "Cache-Control: no-store\r\nConnection: close\r\n%s\r\n",
                      status, type, n, extra ? extra : "");
    send_all(fd, h, (size_t)hl);
    if (n) send_all(fd, body, n);
}

static int qint(const char *q, const char *key, int def)
{
    char k[16];
    snprintf(k, sizeof(k), "%s=", key);
    const char *p = strstr(q, k);
    return p ? atoi(p + strlen(k)) : def;
}

static void handle(int fd)
{
    char req[4096];
    ssize_t n = read(fd, req, sizeof(req) - 1);
    if (n <= 0) return;
    req[n] = 0;
    char path[1024] = "/";
    sscanf(req, "GET %1023s", path);

    if (strcmp(path, "/") == 0 || strncmp(path, "/index", 6) == 0) {
        reply(fd, "200 OK", "text/html; charset=utf-8", NULL, sim_html, strlen(sim_html));
    } else if (strncmp(path, "/frame", 6) == 0) {
        uint32_t since = (uint32_t)qint(path, "since", 0);
        char extra[160];
        snprintf(extra, sizeof(extra), "X-Frame: %u\r\nX-Bright: %d\r\nX-State: %d,%d,%d,%d,%d,%d,%d,%d,%d\r\n", s_frame_no,
                 s_bright, demo_is_enabled(), s_can, demo_is_overheat(), s_page, demo_is_gps(),
                 dash_ui_get_speed_source(), demo_is_knock(), lamps_test(), g_set.road_mode);
        if (since == s_frame_no) reply(fd, "200 OK", "application/octet-stream", extra, NULL, 0);
        else reply(fd, "200 OK", "application/octet-stream", extra, s_fb, sizeof(s_fb));
    } else if (strncmp(path, "/in?", 4) == 0) {
        int nh = (s_qh + 1) % 64;
        if (nh != s_qt) {
            s_q[s_qh] = (ev_t){ (int16_t)qint(path, "x", 0), (int16_t)qint(path, "y", 0), qint(path, "p", 0) != 0 };
            s_qh = nh;
        }
        reply(fd, "204 No Content", "text/plain", NULL, NULL, 0);
    } else if (strncmp(path, "/key?", 5) == 0) {
        const char *k = strstr(path, "k=");
        k = k ? k + 2 : "";
        if (!strcmp(k, "next")) dash_ui_request_page_step(1);
        else if (!strcmp(k, "prev")) dash_ui_request_page_step(-1);
        else if (!strcmp(k, "demo")) demo_set_enabled(!demo_is_enabled());
        else if (!strcmp(k, "can")) s_can = !s_can;
        else if (!strcmp(k, "hot")) demo_set_overheat(!demo_is_overheat());
        else if (!strcmp(k, "gps")) demo_set_gps(!demo_is_gps());
        else if (!strcmp(k, "src")) dash_ui_set_speed_source((dash_ui_get_speed_source() + 1) % 3);
        else if (!strcmp(k, "knock")) demo_set_knock(!demo_is_knock());
        else if (!strcmp(k, "boot")) dash_ui_show_boot();
        else if (!strcmp(k, "turn")) lamps_set_test((lamps_test() & LAMP_HIGH) | ((lamps_test() + 1) & 3));
        else if (!strcmp(k, "high")) lamps_set_test(lamps_test() ^ LAMP_HIGH);
        else if (!strcmp(k, "road")) { g_set.road_mode = !g_set.road_mode; settings_changed(); }
        reply(fd, "204 No Content", "text/plain", NULL, NULL, 0);
    } else if (!strncmp(path, "/web", 4) || !strncmp(path, "/api/", 5) || !strncmp(path, "/logs/", 6)) {
        /* sama veebileht nagu plaadi Wi-Fi-s (web_api.c) */
        web_resp_t r;
        web_api_handle(path, &r);
        if (r.file[0]) {
            FILE *f = fopen(r.file, "rb");
            if (!f) {
                reply(fd, "404 Not Found", "text/plain", NULL, "404", 3);
                return;
            }
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *b = malloc((size_t)n + 1);
            size_t got = fread(b, 1, (size_t)n, f);
            fclose(f);
            char extra[128];
            snprintf(extra, sizeof(extra), "Content-Disposition: attachment; filename=\"%s\"\r\n", r.name);
            reply(fd, "200 OK", r.type, extra, b, got);
            free(b);
        } else {
            reply(fd, r.status == 200 ? "200 OK" : "404 Not Found", r.type, NULL, r.body, r.len);
        }
    } else {
        reply(fd, "404 Not Found", "text/plain", NULL, "404", 3);
    }
}

int main(int argc, char **argv)
{
    int port = argc > 1 ? atoi(argv[1]) : 8090;
    signal(SIGPIPE, SIG_IGN);
    srand((unsigned)time(NULL));

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a = { 0 };
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  /* ainult see arvuti */
    if (bind(ls, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(ls, 16) < 0) {
        perror("port");
        fprintf(stderr, "Port %d on kasutusel? Proovi: ./run.sh 8081\n", port);
        return 1;
    }

    const int64_t t0 = now_us();
    g_fake_time_us = 1000000;
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *d = lv_display_create(SW, SH);
    static uint8_t buf[SW * SH * 2] __attribute__((aligned(64)));
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush_cb);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, touch_cb);

    const dash_ui_cfg_t cfg = { .on_brightness = on_bright, .on_page = on_page, .start_page = 0 };
    dash_ui_create(&cfg);
    logger_start();          /* "SD-kaart" = tools/sim/data/sdcard */
    demo_set_enabled(true);  /* simulaator alustab kohe sõiduga */
    demo_set_gps(true);      /* ja võlts-GPS-iga (nupp "GPS" lülitab välja) */

    printf("SimplDash simulaator töötab:  http://localhost:%d   (Ctrl+C peatab)\n", port);
    fflush(stdout);

    int64_t next_demo = 0;
    for (;;) {
        g_fake_time_us = 1000000 + (now_us() - t0);
        while (g_fake_time_us >= next_demo) {
            if (s_can && demo_is_enabled()) demo_step();
            next_demo = (next_demo ? next_demo : g_fake_time_us) + 20000;
        }
        lv_timer_handler();

        struct pollfd pf = { .fd = ls, .events = POLLIN };
        while (poll(&pf, 1, 5) > 0) {
            int c = accept(ls, NULL, NULL);
            if (c < 0) break;
            handle(c);
            close(c);
            pf.revents = 0;
        }
    }
}
