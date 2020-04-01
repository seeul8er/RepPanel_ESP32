//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <stdio.h>
#include <lvgl/src/lv_objx/lv_cont.h>
#include <lvgl/lvgl.h>
#include <custom_themes/lv_theme_rep_panel_dark.h>
#include "reppanel_console.h"
#include "reppanel.h"
#include "reppanel_request.h"


lv_obj_t *ta_command;
lv_obj_t *user_comm_cont;
lv_obj_t *comm_page;
lv_obj_t *console_page;
lv_obj_t *kb;

console_entry_t console_enties[MAX_CONSOLE_ENTRY_COUNT];
static int num_console_entries = 0;
static int pos_newest_entry = -1;

void _send_user_command() {
    if (strlen(lv_ta_get_text(ta_command)) > 0) {
        char txt[strlen(lv_ta_get_text(ta_command))];
        strcpy(txt, lv_ta_get_text(ta_command));
        printf("Sending command %s\n", txt);
        reprap_send_gcode(txt);
    }
}

static void kb_event_cb(lv_obj_t *event_kb, lv_event_t event) {
    /* Just call the regular event handler */
    lv_kb_def_event_cb(event_kb, event);
    if (event == LV_EVENT_CANCEL) {
        lv_obj_del(kb);
        kb = NULL;
    } else if (event == LV_EVENT_APPLY) {
        _send_user_command();
    }
}

static void ta_event_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        if (kb != NULL) lv_obj_del(kb);
        kb = lv_kb_create(user_comm_cont, NULL);
        lv_obj_set_pos(kb, 5, 90);
        lv_obj_set_event_cb(kb,
                            kb_event_cb); /* Setting a custom event handler stops the keyboard from closing automatically */
        lv_obj_set_size(kb, LV_HOR_RES - 35, 140);
        lv_kb_set_ta(kb, ta_command);
    }
}

static void _send_gcode_event_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        _send_user_command();
    }
}

void _add_entry_to_ui(char *command, char *response, enum console_msg_type type) {
    lv_obj_t *c1 = lv_cont_create(comm_page, NULL);

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
    if (response != NULL && strlen(response) > 0) {
        lv_obj_t *l11 = lv_label_create(c1, NULL);
        static lv_style_t l11_style;
        lv_style_copy(&l11_style, lv_label_get_style(l11, LV_LABEL_STYLE_MAIN));
        l11_style.text.font = &lv_font_roboto_16;
        l11_style.text.color = REP_PANEL_DARK;
        lv_label_set_style(l11, LV_LABEL_STYLE_MAIN, &l11_style);

        lv_label_set_text(l11, response);
    }
}

/**
 * Refresh console history. Must run on UI thread.
 * @param enties
 */
void update_entries_ui() {
    if (comm_page) {
        lv_page_clean(comm_page);
        // get newest entry in buffer
        console_entry_t *entry;
        int indx = pos_newest_entry;
        entry = &console_enties[indx];
        for (int i = 0; i < num_console_entries; i++) {
            if (strlen(entry->command) > 0) {
                _add_entry_to_ui(entry->command, entry->response, entry->type);
                entry--;
                indx--;
                if (indx < 0) {
                    indx = MAX_CONSOLE_ENTRY_COUNT - 1;
                    entry = &console_enties[indx];
                }
            }

        }
    }
}

/**
 * Add a new console history command. Update UI via update_entries_ui()
 * @param command
 * @param response
 * @param type For example CONSOLE_TYPE_WARN | CONSOLE_TYPE_REPPANEL
 */
void add_console_hist_entry(char *command, char *response, enum console_msg_type type) {
    if ((MAX_CONSOLE_ENTRY_COUNT - 1) > pos_newest_entry) {
        pos_newest_entry++;
        num_console_entries++;
    } else
        pos_newest_entry = 0;
    console_entry_t e = {.command = "", .response = "", .type = type};
    strncpy(e.command, command, MAX_LEN_COMMAND);
    if (response != NULL) {
        strncpy(e.response, response, MAX_LEN_RESPONSE);
    }
    console_enties[pos_newest_entry] = e;
}

void draw_console(lv_obj_t *parent_screen) {
    console_page = lv_page_create(parent_screen, NULL);
    lv_obj_set_size(console_page, lv_disp_get_hor_res(NULL),
                    lv_disp_get_ver_res(NULL) - (lv_obj_get_height(cont_header) + 5));
    lv_obj_align(console_page, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_page_set_scrl_layout(console_page, LV_LAYOUT_COL_L);

    user_comm_cont = lv_cont_create(console_page, NULL);
    lv_cont_set_fit(user_comm_cont, LV_FIT_TIGHT);
    lv_cont_set_layout(user_comm_cont, LV_LAYOUT_COL_M);

    lv_obj_t *user_comm_enter_cont = lv_cont_create(user_comm_cont, NULL);
    lv_cont_set_fit(user_comm_enter_cont, LV_FIT_TIGHT);
    lv_cont_set_layout(user_comm_enter_cont, LV_LAYOUT_ROW_M);

    ta_command = lv_ta_create(user_comm_enter_cont, NULL);
    lv_ta_set_cursor_type(ta_command, LV_CURSOR_LINE);
    lv_ta_set_one_line(ta_command, true);
    lv_obj_set_event_cb(ta_command, ta_event_handler);
    lv_ta_set_text(ta_command, "G28");

    static lv_obj_t *btn_send_gcode;
    create_button(user_comm_enter_cont, btn_send_gcode, LV_SYMBOL_UPLOAD" Send", _send_gcode_event_handler);

    lv_obj_set_size(ta_command, lv_disp_get_hor_res(NULL) - 180, 30);

    comm_page = lv_page_create(console_page, NULL);
    lv_obj_set_size(comm_page, lv_disp_get_hor_res(NULL) - 30,
                    lv_disp_get_ver_res(NULL) - (lv_obj_get_height(cont_header) + 80));
    lv_page_set_scrl_layout(comm_page, LV_LAYOUT_COL_L);

    update_entries_ui();
}