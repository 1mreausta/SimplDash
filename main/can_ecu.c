#include "can_ecu.h"
#include <string.h>
#include "settings.h"
#include "store.h"

static const dash_ecu_btn_t BTN[] = DASH_ECU_BUTTONS;
#define N_BTN ((int)(sizeof(BTN) / sizeof(BTN[0])))
_Static_assert(sizeof(BTN) / sizeof(BTN[0]) <= 12, "kuni 12 ECU nuppu");

static struct {
    uint8_t  val[12];          /* TOGGLE/SELECT olek (püsimälus) */
    uint8_t  held[12];         /* HOLD: hetkel all */
    bool     loaded;
    int64_t  next_us;
    uint8_t  last[8];
    bool     last_ok;
    uint32_t sent;
    uint8_t  counter;
} e;

#ifdef DASH_HOST
/* simulaator: CAN-i pole, "saatmine" õnnestub alati */
bool can_send(uint32_t id, const uint8_t *data, uint8_t len);
bool can_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    (void)id; (void)data; (void)len;
    return true;
}
#else
#include "can_rx.h"
#endif

static void load(void)
{
    if (e.loaded) return;
    e.loaded = true;
    if (!store_load("ecubtn", e.val, sizeof(e.val))) memset(e.val, 0, sizeof(e.val));
    for (int i = 0; i < N_BTN; i++) {
        if (BTN[i].kind == BTN_SELECT && (e.val[i] < 1 || e.val[i] > BTN[i].max)) e.val[i] = 1;
        if (BTN[i].kind != BTN_SELECT && e.val[i] > 1) e.val[i] = 0;
    }
}

int can_ecu_count(void) { return N_BTN; }
const dash_ecu_btn_t *can_ecu_button(int i) { return i >= 0 && i < N_BTN ? &BTN[i] : NULL; }

int can_ecu_value(int i)
{
    load();
    if (i < 0 || i >= N_BTN) return 0;
    return BTN[i].kind == BTN_HOLD ? e.held[i] : e.val[i];
}

void can_ecu_press(int i, bool pressed)
{
    load();
    if (i < 0 || i >= N_BTN) return;
    const dash_ecu_btn_t *b = &BTN[i];
    if (b->kind == BTN_HOLD) {
        e.held[i] = pressed;
    } else if (pressed) {
        if (b->kind == BTN_TOGGLE) e.val[i] = !e.val[i];
        else e.val[i] = e.val[i] >= b->max ? 1 : e.val[i] + 1;
        store_save("ecubtn", e.val, sizeof(e.val));
    }
    e.next_us = 0;   /* saada kohe */
}

void can_ecu_last(uint8_t out[8], bool *ok, uint32_t *count)
{
    memcpy(out, e.last, 8);
    *ok = e.last_ok;
    *count = e.sent;
}

void can_ecu_update(const dash_state_t *s)
{
    if (!g_set.can_tx_on || !N_BTN) return;
    load();
    if (e.next_us && s->now_us < e.next_us) return;
    e.next_us = s->now_us + 1000000 / DASH_CAN_TX_HZ;

    uint8_t d[8] = { 0 };
    for (int i = 0; i < N_BTN; i++) {
        const dash_ecu_btn_t *b = &BTN[i];
        if (b->kind == BTN_SELECT) {
            if (b->byte >= 1 && b->byte <= 6) d[b->byte] = e.val[i];
        } else if (b->bit < 8 && (b->kind == BTN_HOLD ? e.held[i] : e.val[i])) {
            d[0] |= (uint8_t)(1u << b->bit);
        }
    }
    d[7] = e.counter++;
    e.last_ok = can_send(DASH_CAN_TX_ID, d, 8);
    memcpy(e.last, d, 8);
    e.sent++;
}
