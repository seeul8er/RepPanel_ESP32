//
// Created by cyber on 29.02.20.
//

#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/src/lv_objx/lv_btn.h>
#include <lvgl/src/lv_objx/lv_label.h>
#include <esp_log.h>
#include <stdio.h>
#include <ctype.h>
#include "esp32_settings.h"
#include "reppanel.h"

char html5[256] = {0};
bool encoding_inited = false;

void url_encoder_rfc_tables_init() {
    int i;
    for (i = 0; i < 256; i++) {
        html5[i] = isalnum (i) || i == '*' || i == '-' || i == '.' || i == '&' || i == '=' || i == '#' || i == '?' ||
                   i == ':' || i == '@' || i == '_' ? i : (i == ' ') ? '+' : 0;
    }
    encoding_inited = true;
}

/**
 * Some basic URL encoding thanks to alian from Stack Overflow
 * @param table
 * @param s
 * @param enc
 * @return
 */
char *url_encode(unsigned char *s, char *enc) {
    if (!encoding_inited) url_encoder_rfc_tables_init();
    for (; *s; s++) {
        if (html5[*s]) sprintf(enc, "%c", html5[*s]);
        else sprintf(enc, "%%%02X", *s);
        while (*++enc);
    }
    return (enc);
}

void init_reprap_buffers() {
    for (int i = 0; i < MAX_NUM_TOOLS; i++) {
        reprap_extruder_amounts[i] = -1;
        reprap_extruder_feedrates[i] = -1;
        reprap_chamber_temp_buff[i] = -1;
    }
    for (int i = 0; i < MAX_NUM_MACROS; i++) {
        reprap_macro_names[i] = NULL;
    }
    for (int i = 0; i < NUM_TEMPS_BUFF; i++) {
        reprap_bed_poss_temps.temps_active[i] = -1;
        reprap_bed_poss_temps.temps_standby[i] = -1;

        reprap_tool_poss_temps.temps_active[i] = -1;
        reprap_tool_poss_temps.temps_standby[i] = -1;
    }
}

void create_button(lv_obj_t *parent, lv_obj_t *button_pnt, char *text, void *event_handler) {
    lv_obj_t *label;
    button_pnt = lv_btn_create(parent, NULL);
    lv_btn_set_fit(button_pnt, LV_FIT_TIGHT);
    lv_obj_set_event_cb(button_pnt, event_handler);
    lv_obj_align(button_pnt, parent, LV_ALIGN_CENTER, 0, 0);
    label = lv_label_create(button_pnt, NULL);
    lv_label_set_text(label, text);
}

void RepPanelLogE(char *tag, char *msg) {
    ESP_LOGE(tag, "%s", msg);
}

void RepPanelLogW(char *tag, char *msg) {
    ESP_LOGW(tag, "%s", msg);
}

void RepPanelLogI(char *tag, char *msg) {
    ESP_LOGI(tag, "%s", msg);
}

void RepPanelLogD(char *tag, char *msg) {
    ESP_LOGD(tag, "%s", msg);
}

void RepPanelLogV(char *tag, char *msg) {
    ESP_LOGV(tag, "%s", msg);
}