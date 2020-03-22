//
// Created by cyber on 21.03.20.
//

#include <stdio.h>
#include <lvgl/src/lv_objx/lv_cont.h>
#include <lvgl/lvgl.h>
#include <lvgl/src/lv_core/lv_style.h>
#include <custom_themes/lv_theme_rep_panel_light.h>
#include "reppanel_console.h"


lv_obj_t *console_container;
lv_obj_t *ta_command;
lv_obj_t *user_comm_cont;
lv_obj_t *console_cont;
lv_obj_t *console_page;
lv_obj_t *kb;

console_entry_t console_enties[MAX_CONSOLE_ENTRY_COUNT];

static void kb_event_cb(lv_obj_t * event_kb, lv_event_t event)
{
    /* Just call the regular event handler */
    lv_kb_def_event_cb(event_kb, event);
    if (event == LV_EVENT_CANCEL) {
        lv_obj_del(kb);
        kb = NULL;
    } else if (event == LV_EVENT_APPLY) {
        printf("Sending command %s\n", lv_ta_get_text(ta_command));
        // TODO: Send GCode
    }
}

static void ta_event_handler(lv_obj_t * obj, lv_event_t event) {
    if(event == LV_EVENT_CLICKED) {
        if(kb != NULL)  lv_obj_del(kb);
        kb = lv_kb_create(user_comm_cont, NULL);
        lv_obj_set_pos(kb, 5, 90);
        lv_obj_set_event_cb(kb, kb_event_cb); /* Setting a custom event handler stops the keyboard from closing automatically */
        lv_obj_set_size(kb,  LV_HOR_RES - 35, 140);
        lv_kb_set_ta(kb, ta_command);
    }
}

void add_entry(char *command, char *response, int type) {
    lv_obj_t *c1 = lv_cont_create(console_page, NULL);

    static lv_style_t entry_style_info;
    lv_style_copy(&entry_style_info, lv_cont_get_style(c1, LV_CONT_STYLE_MAIN));
    entry_style_info.body.main_color = REP_PANEL_DARK_ACCENT;
    entry_style_info.body.grad_color = REP_PANEL_DARK_ACCENT_ALT1;
    entry_style_info.body.padding.inner = LV_DPI / 100;
    static lv_style_t entry_style_warn;
    lv_style_copy(&entry_style_warn, &entry_style_info);
    entry_style_warn.body.main_color = REP_PANEL_DARK_RED;
    entry_style_warn.body.grad_color = REP_PANEL_DARK_DARK_RED;
    static lv_style_t entry_style_reppanel;
    lv_style_copy(&entry_style_reppanel, &entry_style_info);
    entry_style_reppanel.body.main_color = REP_PANEL_DARK_HIGHLIGHT;
    entry_style_reppanel.body.grad_color = REP_PANEL_DARK_HIGHLIGHT_DARK;
    switch (type) {
        default:
        case CONSOLE_TYPE_INFO:
            lv_cont_set_style(c1, LV_CONT_STYLE_MAIN, &entry_style_info);
            break;
        case CONSOLE_TYPE_WARN:
            lv_cont_set_style(c1, LV_CONT_STYLE_MAIN, &entry_style_warn);
            break;
        case CONSOLE_TYPE_REPPANEL:
            lv_cont_set_style(c1, LV_CONT_STYLE_MAIN, &entry_style_reppanel);
            break;
    }
    lv_cont_set_fit2(c1, LV_FIT_FILL, LV_FIT_TIGHT);
    lv_cont_set_layout(c1, LV_LAYOUT_COL_L);
    lv_obj_t *l1 = lv_label_create(c1, NULL);
    static lv_style_t l1_style;
    lv_style_copy(&l1_style, lv_label_get_style(l1, LV_LABEL_STYLE_MAIN));
    l1_style.text.font = &reppanel_font_roboto_bold_16;
    l1_style.text.color = REP_PANEL_DARK;
    lv_label_set_style(l1, LV_LABEL_STYLE_MAIN, &l1_style);
    lv_label_set_text(l1, command);
    if (response != NULL) {
        lv_obj_t *l11 = lv_label_create(c1, NULL);
        static lv_style_t l11_style;
        lv_style_copy(&l11_style, lv_label_get_style(l11, LV_LABEL_STYLE_MAIN));
        l11_style.text.font = &lv_font_roboto_12;
        l11_style.text.color = REP_PANEL_DARK;
        lv_label_set_style(l11, LV_LABEL_STYLE_MAIN, &l11_style);

        lv_label_set_text(l11, response);
    }
}

void update_entries(console_entry_t enties[MAX_CONSOLE_ENTRY_COUNT]) {
    console_entry_t* entry = enties;
    for (int i=0; i < MAX_CONSOLE_ENTRY_COUNT; i++, entry++) {
        if (entry->command != NULL)
            add_entry(entry->command, entry->response, entry->type);
    }
}

void draw_console(lv_obj_t *parent_screen) {
    console_page = lv_page_create(parent_screen, NULL);
    lv_obj_set_size(console_page, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL) - 50);
    lv_obj_align(console_page, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_page_set_scrl_layout(console_page, LV_LAYOUT_COL_M);

    user_comm_cont = lv_cont_create(console_page, NULL);
    lv_cont_set_fit2(user_comm_cont, LV_FIT_FILL, LV_FIT_TIGHT);
    lv_cont_set_layout(user_comm_cont, LV_LAYOUT_COL_M);

    ta_command = lv_ta_create(user_comm_cont, NULL);
    lv_ta_set_cursor_type(ta_command, LV_CURSOR_BLOCK);
    lv_ta_set_one_line(ta_command, true);
    lv_obj_set_size(ta_command, lv_disp_get_hor_res(NULL) - 50, 30);
    lv_obj_set_event_cb(ta_command, ta_event_handler);
    lv_ta_set_text(ta_command, "G28");

    console_cont = lv_cont_create(console_page, NULL);
    lv_cont_set_fit2(console_cont, LV_FIT_FILL, LV_FIT_TIGHT);
    // lv_cont_set_layout(console_cont, LV_LAYOUT_COL_L);

    console_entry_t e = {.command = "M21", .response = NULL, .type = CONSOLE_TYPE_INFO};
    memcpy(&console_enties[0], &e, sizeof(console_entry_t));
    console_entry_t ee = {.command = "M28 X", .response = "Wifi SSID", .type = CONSOLE_TYPE_REPPANEL};
    memcpy(&console_enties[1], &ee, sizeof(console_entry_t));
    console_entry_t eee = {.command = "G01 X0.45 Y452.1", .response = "Moved gantry", .type = CONSOLE_TYPE_WARN};
    memcpy(&console_enties[2], &eee, sizeof(console_entry_t));
    update_entries(console_enties);
}