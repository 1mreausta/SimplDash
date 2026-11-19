#include "store.h"
#include <string.h>

#ifndef DASH_HOST
#include "nvs.h"
#define NS "dash"

static bool load(const char *key, void *buf, size_t len, bool grow)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t n = 0;
    bool ok = nvs_get_blob(h, key, NULL, &n) == ESP_OK && (grow ? n <= len : n == len) &&
              nvs_get_blob(h, key, buf, &n) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool store_load(const char *key, void *buf, size_t len) { return load(key, buf, len, false); }
bool store_load_grow(const char *key, void *buf, size_t len) { return load(key, buf, len, true); }

bool store_save(const char *key, const void *buf, size_t len)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_set_blob(h, key, buf, len) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

void store_erase(const char *key)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_key(h, key);
    nvs_commit(h);
    nvs_close(h);
}

#else
/* Simulaator: kaust keskkonnamuutujast SIMPLDASH_DATA (puudub = ainult mälus, nt eelvaade ja veeb) */
#include <stdio.h>
#include <stdlib.h>

static bool path(const char *key, char *out, size_t cap)
{
    const char *dir = getenv("SIMPLDASH_DATA");
    if (!dir || !*dir) return false;
    snprintf(out, cap, "%s/%s.bin", dir, key);
    return true;
}

static bool load(const char *key, void *buf, size_t len, bool grow)
{
    char p[512];
    if (!path(key, p, sizeof(p))) return false;
    FILE *f = fopen(p, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    bool ok = n > 0 && (grow ? n <= (long)len : n == (long)len) && fread(buf, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    return ok;
}

bool store_load(const char *key, void *buf, size_t len) { return load(key, buf, len, false); }
bool store_load_grow(const char *key, void *buf, size_t len) { return load(key, buf, len, true); }

bool store_save(const char *key, const void *buf, size_t len)
{
    char p[512];
    if (!path(key, p, sizeof(p))) return false;
    FILE *f = fopen(p, "wb");
    if (!f) return false;
    bool ok = fwrite(buf, 1, len, f) == len;
    fclose(f);
    return ok;
}

void store_erase(const char *key)
{
    char p[512];
    if (path(key, p, sizeof(p))) remove(p);
}
#endif
