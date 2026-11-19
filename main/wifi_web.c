#include "wifi_web.h"
#include <stdio.h>
#ifdef DASH_HOST
#include "web_api.h"
void web_lock(void) {}
void web_unlock(void) {}
void web_platform_status(char *buf, size_t cap) { snprintf(buf, cap, "Simulaator – Wi-Fi asemel on see leht aadressil /web."); }
void wifi_web_start(void) {}
void wifi_web_status(char *buf, int cap)
{
    snprintf(buf, cap, "simulaatoris pole (veebileht: /web)");
}
#endif

#ifndef DASH_HOST
/* ---------------------------------------------------------------------------
 * Plaat: ESP32-C6 (SDIO, esp_hosted + esp_wifi_remote) teeb pääsupunkti, HTTP server 192.168.4.1
 * ------------------------------------------------------------------------- */
#include <string.h>
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings.h"
#include "web_api.h"

static const char *TAG = "wifi";
static char s_status[96] = "käivitub…";
static volatile int s_clients;

void web_lock(void) { lvgl_port_lock(0); }
void web_unlock(void) { lvgl_port_unlock(); }

void web_platform_status(char *buf, size_t cap)
{
    snprintf(buf, cap, "Wi-Fi %s, %d klienti. Püsivara: %s", g_set.wifi_ssid, s_clients,
             esp_ota_get_running_partition()->label);
}

void wifi_web_status(char *buf, int cap)
{
    snprintf(buf, cap, "%s", s_status);
}

static esp_err_t on_get(httpd_req_t *req)
{
    if (!strcmp(req->uri, "/api/restart")) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":true,\"msg\":\"\"}");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    static web_resp_t r;     /* httpd teenindab ühte päringut korraga */
    web_api_handle(req->uri, &r);
    httpd_resp_set_type(req, r.type);
    if (r.status != 200) httpd_resp_set_status(req, "404 Not Found");
    if (!r.file[0]) return httpd_resp_send(req, r.body, (ssize_t)r.len);

    FILE *f = fopen(r.file, "rb");
    if (!f) {
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_sendstr(req, "404");
    }
    char disp[96];
    snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", r.name);
    httpd_resp_set_hdr(req, "Content-Disposition", disp);
    static char chunk[8192];
    size_t n;
    esp_err_t err = ESP_OK;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if ((err = httpd_resp_send_chunk(req, chunk, (ssize_t)n)) != ESP_OK) break;
    }
    fclose(f);
    if (err == ESP_OK) httpd_resp_send_chunk(req, NULL, 0);
    return err;
}

/* püsivara uuendus: POST /ota, sisu = simpldash.bin */
static esp_err_t on_ota(httpd_req_t *req)
{
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t h = 0;
    if (!part || esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &h) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA partitsiooni pole (vana partitsioonitabel?)");
        return ESP_FAIL;
    }
    static char buf[4096];
    int left = req->content_len, got_total = 0;
    bool ok = true;
    while (left > 0) {
        int n = httpd_req_recv(req, buf, left < (int)sizeof(buf) ? left : (int)sizeof(buf));
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) { ok = false; break; }
        if (got_total == 0 && (uint8_t)buf[0] != 0xE9) { ok = false; break; }   /* ESP rakenduse päis */
        if (esp_ota_write(h, buf, n) != ESP_OK) { ok = false; break; }
        got_total += n;
        left -= n;
    }
    if (!ok) {
        esp_ota_abort(h);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "vigane fail (vali simpldash.bin)");
        return ESP_FAIL;
    }
    if (esp_ota_end(h) != ESP_OK || esp_ota_set_boot_partition(part) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fail ei läbinud kontrolli");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "OTA ok (%d baiti) → %s, taaskäivitan", got_total, part->label);
    httpd_resp_sendstr(req, "ok");
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
    return ESP_OK;
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)data;
    if (id == WIFI_EVENT_AP_STACONNECTED) s_clients++;
    else if (id == WIFI_EVENT_AP_STADISCONNECTED && s_clients > 0) s_clients--;
    snprintf(s_status, sizeof(s_status), "%s  ·  http://192.168.4.1  ·  %d klienti", g_set.wifi_ssid, s_clients);
}

static void wifi_task(void *arg)
{
    (void)arg;
    esp_err_t err = esp_netif_init();
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) err = ESP_OK;
    if (err == ESP_OK) {
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
    }
    if (err != ESP_OK) {
        snprintf(s_status, sizeof(s_status), "viga: %s (ESP32-C6?)", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", s_status);
        vTaskDelete(NULL);
        return;
    }
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL);
    wifi_config_t ap = { 0 };
    size_t sl = strnlen(g_set.wifi_ssid, sizeof(ap.ap.ssid));
    memcpy(ap.ap.ssid, g_set.wifi_ssid, sl);
    ap.ap.ssid_len = (uint8_t)sl;
    memcpy(ap.ap.password, g_set.wifi_pass, strnlen(g_set.wifi_pass, sizeof(ap.ap.password) - 1));
    ap.ap.channel = 6;
    ap.ap.max_connection = 4;
    ap.ap.authmode = strlen(g_set.wifi_pass) >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    err = esp_wifi_start();
    if (err != ESP_OK) {
        snprintf(s_status, sizeof(s_status), "viga: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    httpd_handle_t srv = NULL;
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.uri_match_fn = httpd_uri_match_wildcard;
    hc.stack_size = 8192;
    hc.recv_wait_timeout = 15;
    hc.core_id = 0;
    if (httpd_start(&srv, &hc) == ESP_OK) {
        httpd_uri_t ota = { .uri = "/ota", .method = HTTP_POST, .handler = on_ota };
        httpd_uri_t get = { .uri = "/*", .method = HTTP_GET, .handler = on_get };
        httpd_register_uri_handler(srv, &ota);
        httpd_register_uri_handler(srv, &get);
    }
    snprintf(s_status, sizeof(s_status), "%s  ·  http://192.168.4.1  ·  %d klienti", g_set.wifi_ssid, s_clients);
    ESP_LOGI(TAG, "pääsupunkt %s, http://192.168.4.1", g_set.wifi_ssid);
    vTaskDelete(NULL);
}

void wifi_web_start(void)
{
    if (!g_set.wifi_on) {
        snprintf(s_status, sizeof(s_status), "väljas (SEADED → Wi-Fi)");
        return;
    }
    xTaskCreatePinnedToCore(wifi_task, "wifi_start", 6144, NULL, 3, NULL, 0);
}
#endif
