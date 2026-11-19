/* Dashi eelvaade arvutis: joonistab iga vaate PPM-failiks (build.sh teeb PNG-d). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "dash_ui.h"
#include "maxxecu.h"
#include "demo.h"
#include "gps.h"
int64_t g_fake_time_us = 1000000;
static bool s_demo;
void demo_set_enabled(bool on){ s_demo = on; }
bool demo_is_enabled(void){ return s_demo; }
static uint32_t fb[1024*600];
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px){
  (void)a; (void)px;
  if (lv_display_flush_is_last(d)) memcpy(fb, lv_display_get_buf_active(d)->data, sizeof(fb));
  lv_display_flush_ready(d);
}
static uint32_t tick(void){ return (uint32_t)(g_fake_time_us/1000); }
static void dump(const char *fn){
  FILE *f=fopen(fn,"wb"); fprintf(f,"P6\n1024 600\n255\n");
  for(int i=0;i<1024*600;i++){ uint32_t c=fb[i]; fputc((c>>16)&255,f); fputc((c>>8)&255,f); fputc(c&255,f);} fclose(f);
}
static float v[SIG_COUNT];
static uint16_t flags;
static bool feeding;
static void feed(void){
  uint8_t b[8];
  for(const uint16_t *id=maxxecu_frame_ids; *id; id++){ maxxecu_encode(*id, v, flags, b); maxxecu_decode(*id, b, 8); }
}
/* võlts-GPS: kiirus gps_kmh (< 0 = GPS puudub), kell 2026-11-19 10:41 UTC + aeg */
static float gps_kmh = -1;
static void gps_line(const char *body){
  unsigned char c = 0; for (const char *p = body; *p; p++) c ^= (unsigned char)*p;
  char l[128]; int n = snprintf(l, sizeof l, "$%s*%02X\r\n", body, c); gps_feed(l, (size_t)n);
}
static void gps_emit(void){
  int64_t ms = g_fake_time_us / 1000; int sec = (int)(ms / 1000);
  char b[128];
  snprintf(b, sizeof b, "GNRMC,10%02d%02d.%02d,A,5923.70000,N,02439.90000,E,%.2f,12.0,191126,,,A",
           41 + sec / 60 % 18, sec % 60, (int)(ms % 1000) / 10, gps_kmh / 1.852);
  gps_line(b);
  gps_line("GNGGA,104100.00,5923.70000,N,02439.90000,E,1,11,0.8,42.0,M,18.0,M,,");
}
static void run(int ms){
  for(int i=0;i<ms/5;i++){ g_fake_time_us += 5000; if(feeding && i%4==0) feed();
    if (gps_kmh >= 0 && (g_fake_time_us / 5000) % 20 == 0) gps_emit(); lv_timer_handler(); }
}
/* hoia vilkumine "sees" faasis ja oota, kuni vaate nime teade kaob */
static void settle(void){ run(1400); while(((g_fake_time_us/250000)%2)==1) run(5); while(((g_fake_time_us/250000)%2)==0) run(5); run(60); }
static const char *NAMES[] = { "race", "kellad", "kaar", "tanav", "0-100", "ring", "andmed" };
int main(void){
  lv_init(); lv_tick_set_cb(tick);
  lv_display_t *d = lv_display_create(1024,600);
  static uint8_t buf[1024*600*4] __attribute__((aligned(64)));
  lv_display_set_color_format(d, LV_COLOR_FORMAT_XRGB8888);
  lv_display_set_buffers(d, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(d, flush);
  dash_ui_cfg_t cfg = { 0 };
  dash_ui_create(&cfg);
  run(1600); dump("0_boot.ppm");          /* käivitusekraan */
  run(1200); dump("0_nocomm.ppm");

  /* tavaline sõit */
  v[SIG_RPM]=5890; v[SIG_TPS]=100; v[SIG_MAP]=210; v[SIG_BARO]=101.3f; v[SIG_LAMBDA]=0.80f;
  v[SIG_SPEED]=147; v[SIG_GEAR]=3; v[SIG_CLT]=91; v[SIG_OIL_TEMP]=112; v[SIG_OIL_PRESS]=480; v[SIG_IAT]=36;
  v[SIG_FUEL_PRESS]=410; v[SIG_BATT]=14.1f; v[SIG_EGT_MAX]=905; v[SIG_IGN_ANGLE]=16; v[SIG_FUEL_DUTY]=78;
  v[SIG_FUEL_TANK]=38.4f; v[SIG_ETHANOL]=0; v[SIG_REVLIMIT_RPM]=7800; v[SIG_FUEL_PW]=9.2f;
  v[SIG_EGT_DIFF]=18; v[SIG_ECU_TEMP]=42; v[SIG_KNOCK_COUNT]=3;
  flags = FLAG_LOGGING|FLAG_ANTILAG; feeding = true;
  /* andmete vaate tippude jaoks natuke varieeruvust */
  float r0 = v[SIG_RPM]; v[SIG_RPM]=7450; v[SIG_SPEED]=188; v[SIG_CLT]=96; run(100);
  v[SIG_RPM]=950; v[SIG_SPEED]=0; v[SIG_CLT]=84; v[SIG_MAP]=35; v[SIG_TPS]=2; run(100);
  v[SIG_RPM]=r0; v[SIG_SPEED]=147; v[SIG_CLT]=91; v[SIG_MAP]=210; v[SIG_TPS]=100;

  /* GPS: seisab 1,5 s, siis kiirendab 0 → 110 km/h (0–100 ≈ 4,55 s) */
  gps_kmh = 0; run(1500);
  for (int i = 1; i <= 50; i++) { gps_kmh = 22.0f * i / 10; if (gps_kmh > 110) gps_kmh = 110; run(100); }
  gps_kmh = 147;
  run(300);

  char fn[64];
  for (int p = 0; p < dash_ui_page_count(); p++) {
    dash_ui_set_page(p); settle();
    snprintf(fn, sizeof(fn), "1_%s.ppm", NAMES[p]); dump(fn);
  }
  /* shift + hoiatus */
  v[SIG_RPM]=7600; v[SIG_CLT]=109; v[SIG_GEAR]=4; flags = FLAG_LOGGING|FLAG_KNOCK;
  for (int p = 0; p < dash_ui_page_count(); p++) {
    dash_ui_set_page(p); settle();
    snprintf(fn, sizeof(fn), "2_%s_hoiatus.ppm", NAMES[p]); dump(fn);
  }
  /* vaate vahetuse teade */
  dash_ui_set_page(1); run(100); dump("3_vahetus.ppm");

  ecu_data_t s; maxxecu_snapshot(&s);
  printf("rpm=%.0f afr=%.2f boost=%.2f oil_press=%.1f flags=0x%x\n", s.v[SIG_RPM], s.v[SIG_AFR], s.v[SIG_BOOST_BAR], s.v[SIG_OIL_PRESS], s.flags);
  return 0;
}
