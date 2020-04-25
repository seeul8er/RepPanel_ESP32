//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <lvgl/src/lv_core/lv_style.h>
#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/src/lv_objx/lv_label.h>
#include <lvgl/lvgl.h>
#include <custom_themes/lv_theme_rep_panel_dark.h>
#include "reppanel.h"
#include "reppanel_process.h"
#include "reppanel_machine.h"
#include "reppanel_info.h"
#include "esp32_wifi.h"
#include "reppanel_macros.h"
#include "reppanel_jobstatus.h"
#include "reppanel_console.h"
#include "reppanel_jobselect.h"
#include <stdio.h>

void draw_header(lv_obj_t *parent_screen);

void draw_main_menu(lv_obj_t *parent_screen);

/**********************
 *  STATIC VARIABLES
 **********************/

uint8_t reppanel_conn_status = REPPANEL_NO_CONNECTION;

lv_obj_t *process_scr;  // screen for the process settings
lv_obj_t *machine_scr;
lv_obj_t *mainmenu_scr; // screen for the main_menue
lv_obj_t *info_scr;     // screen for the info
lv_obj_t *macro_scr;    // macro screen
lv_obj_t *jobstatus_scr;
lv_obj_t *jobselect_scr;
lv_obj_t *console_scr;
lv_obj_t *cont_header;

lv_obj_t *label_status;
lv_obj_t *label_chamber_temp;
lv_obj_t *main_menu_button;
lv_obj_t *console_button;
lv_obj_t *label_connection_status;

char reprap_status;     // printer status
char reppanel_status[MAX_REPRAP_STATUS_LEN];
char reppanel_chamber_temp[MAX_REPRAP_STATUS_LEN];
char reppanel_job_progess[MAX_PREPANEL_TEMP_LEN];

int heater_states[MAX_NUM_TOOLS];
int num_heaters = 1;
bool job_running = false;

void rep_panel_ui_create() {
    lv_theme_t *th = lv_theme_reppanel_dark_init(81, &reppanel_font_roboto_regular_22);
    lv_theme_set_current(th);

    mainmenu_scr = lv_cont_create(NULL, NULL);
    lv_cont_set_layout(mainmenu_scr, LV_LAYOUT_COL_M);
    draw_header(mainmenu_scr);
    draw_main_menu(mainmenu_scr);
    lv_scr_load(mainmenu_scr);
}

static void display_mainmenu_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_RELEASED) {
        if (mainmenu_scr) lv_obj_del(mainmenu_scr);
        mainmenu_scr = lv_cont_create(NULL, NULL);
        lv_cont_set_layout(mainmenu_scr, LV_LAYOUT_COL_M);
        draw_header(mainmenu_scr);
        draw_main_menu(mainmenu_scr);
        lv_scr_load(mainmenu_scr);
    }
}

static void close_conn_info_event_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_mbox_start_auto_close(obj, 0);
    }
}

static void connection_info_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_RELEASED) {
        static const char *btns[] = {"Close", ""};
        char conn_txt[200];
        get_connection_info(conn_txt);
        lv_obj_t *mbox1 = lv_mbox_create(lv_layer_top(), NULL);
        lv_mbox_set_text(mbox1, conn_txt);
        lv_mbox_add_btns(mbox1, btns);
        lv_obj_set_width(mbox1, 250);
        lv_obj_set_event_cb(mbox1, close_conn_info_event_handler);
        lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0); /*Align to the corner*/
    }
}

static void display_console_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_RELEASED) {
        if (console_scr) lv_obj_del(console_scr);
        console_scr = lv_cont_create(NULL, NULL);
        lv_cont_set_layout(console_scr, LV_LAYOUT_COL_M);
        draw_header(console_scr);
        draw_console(console_scr);
        lv_scr_load(console_scr);
    }
}

void display_jobstatus() {
    if (jobstatus_scr) lv_obj_del(jobstatus_scr);
    jobstatus_scr = lv_cont_create(NULL, NULL);
    lv_cont_set_layout(jobstatus_scr, LV_LAYOUT_COL_M);
    draw_header(jobstatus_scr);
    draw_jobstatus(jobstatus_scr);
    lv_scr_load(jobstatus_scr);
}

/**
 * Draw main header at top of screen showing button for main menu navigation
 * @param parent_screen Parent screen to draw elements on
 */
void draw_header(lv_obj_t *parent_screen) {
    cont_header = lv_cont_create(parent_screen, NULL);
    lv_cont_set_fit2(cont_header, LV_FIT_FLOOD, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_header, LV_LAYOUT_OFF);
    lv_obj_align(cont_header, parent_screen, LV_ALIGN_IN_TOP_MID, 0, 0);

    lv_obj_t *cont_header_left = lv_cont_create(cont_header, NULL);
    lv_cont_set_fit(cont_header_left, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_header_left, LV_LAYOUT_ROW_M);
    lv_obj_set_event_cb(cont_header_left, display_mainmenu_event);
    lv_obj_align(cont_header_left, cont_header, LV_ALIGN_IN_TOP_LEFT, 10, 200);


    LV_IMG_DECLARE(mainmenubutton);
    static lv_style_t style_main_button;
    lv_style_copy(&style_main_button, &lv_style_plain);
    style_main_button.image.color = LV_COLOR_BLACK;
    style_main_button.image.intense = LV_OPA_50;
    style_main_button.text.color = lv_color_hex3(0xaaa);

    main_menu_button = lv_imgbtn_create(cont_header_left, NULL);
    lv_imgbtn_set_src(main_menu_button, LV_BTN_STATE_REL, &mainmenubutton);
    lv_imgbtn_set_src(main_menu_button, LV_BTN_STATE_PR, &mainmenubutton);
    lv_imgbtn_set_src(main_menu_button, LV_BTN_STATE_TGL_REL, &mainmenubutton);
    lv_imgbtn_set_src(main_menu_button, LV_BTN_STATE_TGL_PR, &mainmenubutton);
    lv_imgbtn_set_style(main_menu_button, LV_BTN_STATE_PR, &style_main_button);
    lv_imgbtn_set_style(main_menu_button, LV_BTN_STATE_TGL_PR, &style_main_button);
    lv_imgbtn_set_toggle(main_menu_button, false);
    lv_obj_set_event_cb(main_menu_button, display_mainmenu_event);

    label_status = lv_label_create(cont_header_left, NULL);
    static lv_style_t style_status_label;
    lv_style_copy(&style_status_label, &lv_style_plain);
    style_status_label.text.color = REP_PANEL_DARK_ACCENT;
    style_status_label.text.font = &reppanel_font_roboto_bold_24;
    lv_obj_set_style(label_status, &style_status_label);
    lv_label_set_text(label_status, reppanel_status);

    lv_obj_t *cont_header_right = lv_cont_create(cont_header, NULL);
    lv_cont_set_fit(cont_header_right, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_header_right, LV_LAYOUT_ROW_M);
    lv_obj_align(cont_header_right, cont_header, LV_ALIGN_IN_TOP_RIGHT, -120, 12);

    lv_obj_t *click_cont = lv_cont_create(cont_header_right, NULL);
    lv_cont_set_fit(click_cont, LV_FIT_TIGHT);
    label_connection_status = lv_label_create(click_cont, NULL);
    lv_label_set_recolor(label_connection_status, true);
    update_rep_panel_conn_status();
    lv_obj_set_event_cb(click_cont, connection_info_event);

    lv_obj_t *img_chamber_tmp = lv_img_create(cont_header_right, NULL);
    LV_IMG_DECLARE(chamber_tmp);
    lv_img_set_src(img_chamber_tmp, &chamber_tmp);

    label_chamber_temp = lv_label_create(cont_header_right, NULL);
    lv_label_set_text_fmt(label_chamber_temp, "%.01f°%c",
                          reprap_tools[current_visible_tool_indx].temp_buff[reprap_tools[current_visible_tool_indx].temp_hist_curr_pos],
                          get_temp_unit());

    LV_IMG_DECLARE(consolebutton);
    static lv_style_t style_console_button;
    lv_style_copy(&style_console_button, &lv_style_plain);
    style_console_button.image.color = LV_COLOR_BLACK;
    style_console_button.image.intense = LV_OPA_50;
    style_console_button.text.color = lv_color_hex3(0xaaa);

    console_button = lv_imgbtn_create(cont_header_right, NULL);
    lv_imgbtn_set_src(console_button, LV_BTN_STATE_REL, &consolebutton);
    lv_imgbtn_set_src(console_button, LV_BTN_STATE_PR, &consolebutton);
    lv_imgbtn_set_src(console_button, LV_BTN_STATE_TGL_REL, &consolebutton);
    lv_imgbtn_set_src(console_button, LV_BTN_STATE_TGL_PR, &consolebutton);
    lv_imgbtn_set_style(console_button, LV_BTN_STATE_PR, &style_console_button);
    lv_imgbtn_set_style(console_button, LV_BTN_STATE_TGL_PR, &style_console_button);
    lv_imgbtn_set_toggle(console_button, true);
    lv_obj_set_event_cb(console_button, display_console_event);
}

void update_rep_panel_conn_status() {
    if (label_connection_status) {
        switch (reppanel_conn_status) {
            default:
            case REPPANEL_NO_CONNECTION:     // no connection
                lv_label_set_text_fmt(label_connection_status, "#e84e43 "LV_SYMBOL_WARNING"#");
                break;
            case REPPANEL_WIFI_CONNECTED:     // connected wifi
                lv_label_set_text_fmt(label_connection_status, REP_PANEL_DARK_ACCENT_STR" "LV_SYMBOL_WIFI);
                break;
            case REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED:
                lv_label_set_text_fmt(label_connection_status, "#ff8921 "LV_SYMBOL_WIFI);
                break;
            case REPPANEL_WIFI_DISCONNECTED:     // disconnected wifi
                lv_label_set_text_fmt(label_connection_status, "#e84e43 "LV_SYMBOL_WIFI);
                break;
            case REPPANEL_WIFI_RECONNECTING:     // reconnecting wifi
                lv_label_set_text_fmt(label_connection_status, "#e89e43 "LV_SYMBOL_REFRESH);
                break;
            case REPPANEL_UART_CONNECTED:     // working UART
                lv_label_set_text_fmt(label_connection_status, REP_PANEL_DARK_ACCENT_STR" "LV_SYMBOL_USB);
                break;
        }
    }
}

static void _show_process_screen(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        update_rep_panel_conn_status();
        if (process_scr) lv_obj_del(process_scr);
        process_scr = lv_cont_create(NULL, NULL);
        lv_cont_set_layout(process_scr, LV_LAYOUT_COL_M);
        draw_header(process_scr);
        draw_process(process_scr);
        lv_scr_load(process_scr);
    }
}

static void _show_job_screen(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        update_rep_panel_conn_status();
        if (job_running) {
            display_jobstatus();
        } else {
            if (jobselect_scr) lv_obj_del(jobselect_scr);
            jobselect_scr = lv_cont_create(NULL, NULL);
            lv_cont_set_layout(jobselect_scr, LV_LAYOUT_COL_M);
            draw_header(jobselect_scr);
            draw_jobselect(jobselect_scr);
            lv_scr_load(jobselect_scr);
        }
    }
}

static void _show_machine_screen(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        update_rep_panel_conn_status();
        if (machine_scr) lv_obj_del(machine_scr);
        machine_scr = lv_cont_create(NULL, NULL);
        lv_cont_set_layout(machine_scr, LV_LAYOUT_COL_M);
        draw_header(machine_scr);
        draw_machine(machine_scr);
        lv_scr_load(machine_scr);
    }
}

static void _show_macros_screen(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        update_rep_panel_conn_status();
        if (info_scr) lv_obj_del(info_scr);
        macro_scr = lv_cont_create(NULL, NULL);
        lv_cont_set_layout(macro_scr, LV_LAYOUT_COL_M);
        draw_header(macro_scr);
        draw_macro(macro_scr);
        lv_scr_load(macro_scr);
    }
}

static void _show_info_screen(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        update_rep_panel_conn_status();
        if (info_scr) lv_obj_del(info_scr);
        info_scr = lv_cont_create(NULL, NULL);
        lv_cont_set_layout(info_scr, LV_LAYOUT_COL_M);
        draw_header(info_scr);
        draw_info(info_scr);
        lv_scr_load(info_scr);
    }
}

void draw_main_menu(lv_obj_t *parent_screen) {
    lv_obj_t *cont_row1 = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(cont_row1, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(cont_row1, LV_FIT_TIGHT);
    static lv_style_t cont_row1_style;
    lv_style_copy(&cont_row1_style, lv_cont_get_style(cont_row1, LV_CONT_STYLE_MAIN));
    cont_row1_style.body.padding.inner = LV_DPI / 5;
    cont_row1_style.body.padding.top = LV_DPI / 5;
    cont_row1_style.body.padding.bottom = LV_DPI / 10;
    lv_cont_set_style(cont_row1, LV_CONT_STYLE_MAIN, &cont_row1_style);

    lv_obj_t *cont_row2 = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(cont_row2, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(cont_row2, LV_FIT_TIGHT);

    LV_IMG_DECLARE(process_icon);
    lv_obj_t *button_main_menu_process = lv_btn_create(cont_row1, NULL);
    lv_btn_set_layout(button_main_menu_process, LV_LAYOUT_OFF);
    lv_obj_set_event_cb(button_main_menu_process, _show_process_screen);
    lv_obj_t *img_process = lv_img_create(button_main_menu_process, NULL);
    lv_img_set_src(img_process, &process_icon);
    lv_obj_t *label_process = lv_label_create(button_main_menu_process, NULL);
    lv_label_set_text(label_process, "Process");
    lv_obj_set_height(button_main_menu_process, 140);
    lv_obj_set_width(button_main_menu_process, 130);
    lv_obj_align(label_process, button_main_menu_process, LV_ALIGN_IN_BOTTOM_MID, -7, -10);
    lv_obj_align_origo(img_process, button_main_menu_process, LV_ALIGN_CENTER, 0, -15);
    // style buttons process & machine
    static lv_style_t main_button_style_rel;
    lv_style_copy(&main_button_style_rel, lv_cont_get_style(button_main_menu_process, LV_BTN_STYLE_REL));
    main_button_style_rel.body.radius = 0;
    main_button_style_rel.text.font = &reppanel_font_roboto_regular_26;
    lv_btn_set_style(button_main_menu_process, LV_BTN_STYLE_REL, &main_button_style_rel);
    static lv_style_t main_button_style_pr;
    lv_style_copy(&main_button_style_pr, lv_cont_get_style(button_main_menu_process, LV_BTN_STYLE_PR));
    main_button_style_pr.body.main_color = REP_PANEL_DARK;
    main_button_style_pr.body.grad_color = REP_PANEL_DARK;
    main_button_style_pr.body.border.color = REP_PANEL_DARK_ACCENT;
    main_button_style_pr.body.border.width = 1;
    main_button_style_pr.body.shadow.width = 0;
    main_button_style_pr.text.color = REP_PANEL_DARK_ACCENT;
    lv_btn_set_style(button_main_menu_process, LV_BTN_STYLE_PR, &main_button_style_pr);

    LV_IMG_DECLARE(job_icon);
    lv_obj_t *button_main_menu_job = lv_btn_create(cont_row1, button_main_menu_process);
    lv_obj_set_event_cb(button_main_menu_job, _show_job_screen);
    lv_obj_t *img_job = lv_img_create(button_main_menu_job, NULL);
    lv_img_set_src(img_job, &job_icon);
    lv_obj_t *label_job = lv_label_create(button_main_menu_job, NULL);
    lv_label_set_text(label_job, "Job");
    lv_obj_align(label_job, button_main_menu_job, LV_ALIGN_IN_BOTTOM_MID, 0, -10);
    lv_obj_align_origo(img_job, button_main_menu_job, LV_ALIGN_CENTER, 0, -15);
    // style button job
    static lv_style_t job_button_style_rel;
    lv_style_copy(&job_button_style_rel, &main_button_style_rel);
    job_button_style_rel.body.radius = 0;
    job_button_style_rel.body.main_color = REP_PANEL_DARK_ACCENT_ALT2;
    job_button_style_rel.body.grad_color = REP_PANEL_DARK_ACCENT_ALT2;
    lv_btn_set_style(button_main_menu_job, LV_BTN_STYLE_REL, &job_button_style_rel);

    LV_IMG_DECLARE(machine_icon);
    lv_obj_t *button_main_menu_machine = lv_btn_create(cont_row1, button_main_menu_process);
    lv_obj_set_event_cb(button_main_menu_machine, _show_machine_screen);
    lv_obj_t *img_machine = lv_img_create(button_main_menu_machine, NULL);
    lv_img_set_src(img_machine, &machine_icon);
    lv_obj_t *label_machine = lv_label_create(button_main_menu_machine, NULL);
    lv_label_set_text(label_machine, "Machine");
    lv_obj_align(label_machine, button_main_menu_machine, LV_ALIGN_IN_BOTTOM_MID, 0, -10);
    lv_obj_align_origo(img_machine, button_main_menu_machine, LV_ALIGN_CENTER, 0, -15);

    LV_IMG_DECLARE(macro_icon);
    lv_obj_t *button_main_menu_macro = lv_btn_create(cont_row2, NULL);
    lv_btn_set_layout(button_main_menu_macro, LV_LAYOUT_OFF);
    lv_obj_set_event_cb(button_main_menu_macro, _show_macros_screen);
    lv_obj_t *img_macro = lv_img_create(button_main_menu_macro, NULL);
    lv_img_set_src(img_macro, &macro_icon);
    lv_obj_t *label_macro = lv_label_create(button_main_menu_macro, NULL);
    lv_label_set_text(label_macro, "Macros");
    lv_obj_set_height(button_main_menu_macro, 55);
    lv_obj_set_width(button_main_menu_macro, 210);
    lv_obj_align_origo(label_macro, button_main_menu_macro, LV_ALIGN_CENTER, 14, 0);
    lv_obj_align_origo(img_macro, button_main_menu_macro, LV_ALIGN_CENTER, -52, 0);
    lv_btn_set_style(button_main_menu_macro, LV_BTN_STYLE_REL, &main_button_style_rel);
    lv_btn_set_style(button_main_menu_macro, LV_BTN_STYLE_PR, &main_button_style_pr);

    LV_IMG_DECLARE(info_icon);
    lv_obj_t *button_main_menu_info = lv_btn_create(cont_row2, button_main_menu_macro);
    lv_obj_set_event_cb(button_main_menu_info, _show_info_screen);
    lv_obj_t *img_info = lv_img_create(button_main_menu_info, NULL);
    lv_img_set_src(img_info, &info_icon);
    lv_obj_t *label_info = lv_label_create(button_main_menu_info, NULL);
    lv_label_set_text(label_info, "Info");
    lv_obj_align_origo(label_info, button_main_menu_info, LV_ALIGN_CENTER, 14, 0);
    lv_obj_align_origo(img_info, button_main_menu_info, LV_ALIGN_CENTER, -32, 0);
    lv_btn_set_style(button_main_menu_info, LV_BTN_STYLE_REL, &main_button_style_rel);
    lv_btn_set_style(button_main_menu_info, LV_BTN_STYLE_PR, &main_button_style_pr);
}