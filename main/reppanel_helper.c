//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/src/lv_objx/lv_btn.h>
#include <lvgl/src/lv_objx/lv_label.h>
#include <esp_log.h>
#include <stdio.h>
#include <ctype.h>
#include "esp32_settings.h"
#include "reppanel.h"
#ifdef CONFIG_REPPANEL_ESP32_CONSOLE_ENABLED
#include "reppanel_console.h"
#endif

char html5[256] = {0};
bool encoding_inited = false;
lv_obj_t *cont_msg;

char *get_version_string() {
    static char version_str[12];
    sprintf(version_str, "%i.%i.%i", VERSION_MAJOR, VERSION_MINOR, VERSION_HOTFIX);
    return version_str;
}

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

bool ends_with(const char *base, char *str) {
    int blen = strlen(base);
    int slen = strlen(str);
    return (blen >= slen) && (0 == strcmp(base + blen - slen, str));
}

void init_reprap_buffers() {
    for (int i = 0; i < MAX_NUM_TOOLS; i++) {
        reprap_extruder_amounts[i] = -1;
        reprap_extruder_feedrates[i] = -1;
        reprap_chamber_temp_buff[i] = -1;
    }
    for (int i = 0; i < MAX_NUM_ELEM_DIR; i++) {
        reprap_dir_elem[i].type = TREE_EMPTY_ELEM;
    }
#ifdef CONFIG_REPPANEL_ESP32_CONSOLE_ENABLED
    for (int i = 0; i < MAX_CONSOLE_ENTRY_COUNT; i++) {
        console_enties[i] = (console_entry_t) {"", CONSOLE_TYPE_EMPTY};
    }
#endif

    reprap_tool_poss_temps.temps_active[0] = -1;
    memset(&reprap_axes, 0, sizeof (reprap_axes_t));
    double bed_temps_hardcoded[] = {0, 30, 53, 60, 80, 100, 105, 110, -1};  // max len 15, last must be <0
    double tool_temps_hardcoded[] = {0, 160, 180, 185, 190, 200, 210, 250, 265, 280,
                                     -1};  // max len 15, last must be <0
    memcpy(reprap_bed_poss_temps.temps_standby, bed_temps_hardcoded, sizeof(bed_temps_hardcoded));
    memcpy(reprap_bed_poss_temps.temps_active, bed_temps_hardcoded, sizeof(bed_temps_hardcoded));
    memcpy(reprap_tool_poss_temps.temps_standby, tool_temps_hardcoded, sizeof(tool_temps_hardcoded));
    // memcpy(reprap_tool_poss_temps.temps_active, tool_temps_hardcoded, sizeof(tool_temps_hardcoded));
}

lv_obj_t *create_button(lv_obj_t *parent, lv_obj_t *button_pnt, char *text, void *event_handler) {
    lv_obj_t *label;
    button_pnt = lv_btn_create(parent, NULL);
    lv_btn_set_fit(button_pnt, LV_FIT_TIGHT);
    lv_obj_set_event_cb(button_pnt, event_handler);
    lv_obj_align(button_pnt, parent, LV_ALIGN_CENTER, 0, 0);
    label = lv_label_create(button_pnt, NULL);
    lv_label_set_text(label, text);
    return button_pnt;
}

time_t datestr_2unix(const char *input) {
    struct tm info;
    if (sscanf(input, "%d-%d-%dT%d:%d:%d", &info.tm_year, &info.tm_mon, &info.tm_mday, &info.tm_hour, &info.tm_min,
               &info.tm_sec) != 6) {
        return -1;
    } else {
        info.tm_year = info.tm_year - 1900;
        info.tm_mon = info.tm_mon - 1;
        info.tm_isdst = -1;
        return mktime(&info);
    }
}

/**
 * Compare function for qsort. Compares file_tree_elem_t structures by modification date/time stamp. Most recent element
 * is first in order (highest number first)
 * @param a type file_tree_elem_t
 * @param b type file_tree_elem_t
 * @return see qsort docs
 */
int compare_tree_element_timestamp(const void *a, const void *b) {
    file_tree_elem_t *tree_elemA = (file_tree_elem_t *) a;
    file_tree_elem_t *tree_elemB = (file_tree_elem_t *) b;
    if (tree_elemA->time_stamp - tree_elemB->time_stamp < 0)
        return 1;
    if (tree_elemA->time_stamp - tree_elemB->time_stamp > 0)
        return -1;
    return 0;
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