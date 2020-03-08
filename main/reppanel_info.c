//
// Created by cyber on 01.03.20.
//

#include <stdio.h>
#include <lvgl/src/lv_objx/lv_page.h>
#include <lvgl/lvgl.h>
#include <custom_themes/lv_theme_rep_panel_light.h>
#include <esp_log.h>
#include "reppanel_info.h"
#include "reppanel.h"

#define TAG "RepPanelInfo"

lv_obj_t *info_page, *label_sig_strength, *save_bnt;
lv_obj_t *ta_wifi_pass;
lv_obj_t *ta_ssid;
lv_obj_t *ta_printer_addr;
lv_obj_t *ta_reprap_pass;

static lv_obj_t *kb;

static void save_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Saving settings\n");
        const char *ssid = lv_ta_get_text(ta_ssid);
        strncpy(wifi_ssid, ssid, 32);
        const char *tmp_wifi_pass = lv_ta_get_text(ta_wifi_pass);
        strncpy(wifi_pass, tmp_wifi_pass, 64);

        const char *tmp_rep_addr = lv_ta_get_text(ta_printer_addr);
        strncpy(rep_addr, tmp_rep_addr, 200);
        const char *tmp_rep_pass = lv_ta_get_text(ta_reprap_pass);
        strncpy(rep_pass, tmp_rep_pass, 64);
        write_settings_to_nvs();
    }
}

static void kb_event_cb(lv_obj_t *event_kb, lv_event_t event) {
    /* Just call the regular event handler */
    lv_kb_def_event_cb(event_kb, event);
    if (event == LV_EVENT_CANCEL) {
        lv_obj_del(kb);
        kb = NULL;
    }
}

static void ta_event_cb(lv_obj_t *ta, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        /* Focus on the clicked text area */
        if (kb == NULL) {
            kb = lv_kb_create(info_page, NULL);
            lv_obj_set_pos(kb, 0, 0);
            lv_obj_set_size(kb, LV_HOR_RES - 50, 95);
            lv_kb_set_cursor_manage(kb, true); /* Automatically show/hide cursors on text areas */
            lv_kb_set_ta(kb, ta);
            lv_obj_set_event_cb(kb, kb_event_cb);
        } else {
            lv_kb_set_ta(kb, ta);
        }
    } else if (event == LV_EVENT_INSERT) {
        const char *str = lv_event_get_data();
        if (str[0] == '\n') {
            printf("Ready\n");
        }
    }
}

void draw_info(lv_obj_t *parent_screen) {
    info_page = lv_page_create(parent_screen, NULL);
    lv_obj_set_size(info_page, lv_disp_get_hor_res(NULL), 250);
    lv_page_set_scrl_fit2(info_page, LV_FIT_TIGHT, LV_FIT_FILL);
    lv_page_set_scrl_layout(info_page, LV_LAYOUT_COL_L);

    lv_obj_t *label_about = lv_label_create(info_page, NULL);
    lv_label_set_recolor(label_about, true);
    lv_label_set_text_fmt(label_about, "%s RepPanel for ESP32# - %s", REP_PANEL_DARK_ACCENT_ALT1_STR, VERSION_STR);

    lv_obj_t *ssid_cnt = lv_cont_create(info_page, NULL);
    lv_cont_set_layout(ssid_cnt, LV_LAYOUT_ROW_T);
    lv_cont_set_fit(ssid_cnt, LV_FIT_TIGHT);
    lv_obj_t *label_ssid = lv_label_create(ssid_cnt, NULL);
    lv_label_set_text(label_ssid, "WiFi SSID:");

    ta_ssid = lv_ta_create(ssid_cnt, NULL);
    lv_ta_set_pwd_mode(ta_ssid, false);
    lv_ta_set_cursor_type(ta_ssid, LV_CURSOR_LINE | LV_CURSOR_HIDDEN);
    lv_obj_set_width(ta_ssid, LV_HOR_RES - 250);
    lv_obj_set_height(ta_ssid, 25);
    lv_ta_set_text(ta_ssid, (const char *) wifi_ssid);
    lv_obj_set_event_cb(ta_ssid, ta_event_cb);
    lv_ta_set_one_line(ta_ssid, true);

    lv_obj_t *wifi_pass_cnt = lv_cont_create(info_page, ssid_cnt);
    lv_obj_t *label_wifi_pass = lv_label_create(wifi_pass_cnt, NULL);
    lv_label_set_text(label_wifi_pass, "WiFi password:");

    ta_wifi_pass = lv_ta_create(wifi_pass_cnt, NULL);
    lv_ta_set_text(ta_wifi_pass, (const char *) wifi_pass);
    lv_ta_set_pwd_mode(ta_wifi_pass, true);
    lv_ta_set_one_line(ta_wifi_pass, true);
    lv_obj_set_width(ta_wifi_pass, LV_HOR_RES - 250);
    lv_obj_set_pos(ta_wifi_pass, 0, 0);
    lv_obj_set_event_cb(ta_wifi_pass, ta_event_cb);
    lv_ta_set_one_line(ta_wifi_pass, true);

    lv_obj_t *printer_addr_cnt = lv_cont_create(info_page, ssid_cnt);
    lv_obj_t *label_printer_addr = lv_label_create(printer_addr_cnt, NULL);
    lv_label_set_text(label_printer_addr, "Printer address:");

    ta_printer_addr = lv_ta_create(printer_addr_cnt, NULL);
    lv_ta_set_pwd_mode(ta_printer_addr, false);
    lv_ta_set_cursor_type(ta_printer_addr, LV_CURSOR_LINE | LV_CURSOR_HIDDEN);
    lv_obj_set_width(ta_printer_addr, LV_HOR_RES - 250);
    lv_obj_set_height(ta_printer_addr, 25);
    lv_ta_set_text(ta_printer_addr, (const char *) wifi_ssid);
    lv_obj_set_event_cb(ta_printer_addr, ta_event_cb);
    lv_ta_set_one_line(ta_ssid, true);

    lv_obj_t *reprap_pass_cnt = lv_cont_create(info_page, ssid_cnt);
    lv_obj_t *label_reprap_pass = lv_label_create(reprap_pass_cnt, NULL);
    lv_label_set_text(label_reprap_pass, "RepRap password:");

    ta_reprap_pass = lv_ta_create(reprap_pass_cnt, NULL);
    lv_ta_set_text(ta_reprap_pass, (const char *) wifi_pass);
    lv_ta_set_pwd_mode(ta_reprap_pass, true);
    lv_ta_set_one_line(ta_reprap_pass, true);
    lv_obj_set_width(ta_reprap_pass, LV_HOR_RES - 260);
    lv_obj_set_pos(ta_reprap_pass, 0, 0);
    lv_obj_set_event_cb(ta_reprap_pass, ta_event_cb);
    lv_ta_set_one_line(ta_reprap_pass, true);

//    label_sig_strength = lv_label_create(info_page, NULL);
//    lv_label_set_recolor(label_sig_strength, true);
//    lv_label_set_long_mode(label_sig_strength, LV_LABEL_LONG_BREAK);
//    lv_label_set_text_fmt(label_sig_strength, "RepRap signal: %s-124dBm# RepPanel signal: %s-125dBm#",
//            REP_PANEL_DARK_ACCENT_ALT2_STR, REP_PANEL_DARK_ACCENT_ALT2_STR);
    create_button(info_page, save_bnt, "Save", save_event);


}