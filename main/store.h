/*
 * Püsimälu: väikesed andmeplokid võtme järgi (plaadil NVS, simulaatoris failid).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Loe plokk. false = puudub või suurus ei klapi (siis buf jääb puutumata). */
bool store_load(const char *key, void *buf, size_t len);
/* Kirjuta plokk (key ≤ 15 märki). */
bool store_save(const char *key, const void *buf, size_t len);
/* Kustuta plokk. */
void store_erase(const char *key);
