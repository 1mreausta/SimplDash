/*
 * ECU paneel (SEADED vaate esimene vahekaart): nupud, mis saadavad MaxxECU-le CAN-kaadri (DASH_ECU_BUTTONS, dash_config.h).
 * All ECU tagasiside: olekulipud ja aktiivne kaart (MaxxECU 0x540).
 */
#include <stdio.h>
#include <string.h>
#include "can_ecu.h"
#include "dash_common.h"

#define MAXB 12

static struct {
    lv_obj_t *btn[MAXB], *lbl[MAXB], *val[MAXB];
    char      t_val[MAXB][16];
    int8_t    on[MAXB];
    lv_obj_t *status, *tune;
    char      t_status[96], t_tune[48];
    dash_flags_t flags;
} p;

static void on_btn(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t c = lv_event_get_code(e);
    bool hold = can_ecu_button(i)->kind == BTN_HOLD;
    /* HOLD: kogu vajutuse aja; lüliti/valik: alles klõpsul (libistus üle nupu ei muuda väärtust) */
    if (hold && c == LV_EVENT_PRESSED) can_ecu_press(i, true);
    else if (hold && (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST)) can_ecu_press(i, false);
    else if (!hold && c == LV_EVENT_CLICKED) can_ecu_press(i, true);
}

void ecu_panel_create(lv_obj_t *scr)
{
    const int vh = H - 70;   /* vahekaardi kõrgus (page_settings.c view_box) */
    p.status = dash_label(scr, 18, C_DIM, "");
    lv_obj_set_pos(p.status, 16, 4);

    const int n = can_ecu_count() < MAXB ? can_ecu_count() : MAXB;
    const int cols = 3, bw = 320, bh = 120, gx = 18, gy = 16, x0 = (W - cols * bw - (cols - 1) * gx) / 2, y0 = 36;
    for (int i = 0; i < n; i++) {
        const dash_ecu_btn_t *b = can_ecu_button(i);
        lv_obj_t *o = lv_button_create(scr);
        lv_obj_remove_style_all(o);
        lv_obj_set_size(o, bw, bh);
        lv_obj_set_pos(o, x0 + (i % cols) * (bw + gx), y0 + (i / cols) * (bh + gy));
        lv_obj_set_style_bg_color(o, C_PANEL, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(o, lv_color_hex(0x1B2633), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(o, C_LINE, 0);
        lv_obj_set_style_border_width(o, 3, 0);
        lv_obj_set_style_radius(o, 12, 0);
        lv_obj_add_event_cb(o, on_btn, LV_EVENT_PRESSED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(o, on_btn, LV_EVENT_RELEASED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(o, on_btn, LV_EVENT_PRESS_LOST, (void *)(intptr_t)i);
        lv_obj_add_event_cb(o, on_btn, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_set_event_bubble(o, true);   /* libistamine nupu pealt vahetab vaadet */
        p.lbl[i] = dash_label(o, 26, C_DIM, b->label);
        lv_obj_align(p.lbl[i], LV_ALIGN_TOP_MID, 0, 12);
        p.val[i] = dash_label(o, 50, C_TEXT, "");
        lv_obj_align(p.val[i], LV_ALIGN_BOTTOM_MID, 0, -8);
        p.btn[i] = o;
        p.on[i] = -1;
    }
    p.tune = dash_label(scr, 22, C_DIM, "");
    lv_obj_set_pos(p.tune, 20, vh - 46);
    dash_flags_create(&p.flags, scr, W - 200, vh - 44);
}

void ecu_panel_update(const dash_state_t *s)
{
    char buf[96];
    const int n = can_ecu_count() < MAXB ? can_ecu_count() : MAXB;
    for (int i = 0; i < n; i++) {
        const dash_ecu_btn_t *b = can_ecu_button(i);
        int v = can_ecu_value(i);
        if (b->kind == BTN_SELECT) snprintf(buf, 16, "%d", v);
        else snprintf(buf, 16, "%s", v ? "SEES" : "VÄLJAS");
        dash_set_text_if(p.val[i], p.t_val[i], 16, buf);
        int8_t on = b->kind == BTN_SELECT ? 2 : v ? 1 : 0;
        if (on != p.on[i]) {
            lv_color_t c = on == 1 ? C_GREEN : on == 2 ? C_ACCENT : C_LINE;
            lv_obj_set_style_border_color(p.btn[i], c, 0);
            lv_obj_set_style_text_color(p.val[i], on == 0 ? C_DIM : C_TEXT, 0);
            p.on[i] = on;
        }
    }
    if (!g_set.can_tx_on) {
        snprintf(buf, sizeof(buf), "saatmine VÄLJAS – lülita sisse: SEADED vahekaart, jaotis ECU (vt README)");
    } else {
        uint8_t d[8];
        bool ok;
        uint32_t cnt;
        can_ecu_last(d, &ok, &cnt);
        snprintf(buf, sizeof(buf), "saadan ID 0x%03X, %d Hz  ·  %02X %02X %02X %02X %02X %02X %02X %02X%s",
                 DASH_CAN_TX_ID, DASH_CAN_TX_HZ, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
                 cnt && !ok ? "  ·  VIGA" : "");
    }
    dash_set_text_if(p.status, p.t_status, sizeof(p.t_status), buf);
    if (s->d.valid[SIG_TUNE_SELECTOR])
        snprintf(buf, sizeof(buf), "ECU: seade %d  ·  boost seade %d", (int)s->d.v[SIG_TUNE_SELECTOR],
                 (int)s->d.v[SIG_BOOST_TABLE]);
    else
        buf[0] = 0;
    dash_set_text_if(p.tune, p.t_tune, sizeof(p.t_tune), buf);
    dash_flags_update(&p.flags, s);
}
