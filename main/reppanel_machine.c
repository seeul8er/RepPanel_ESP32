//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include "freertos/FreeRTOS.h"
#include <freertos/task.h>
#include <lvgl/src/lv_objx/lv_page.h>
#include <lvgl/src/lv_objx/lv_ddlist.h>
#include <esp_log.h>
#include <lvgl/lvgl.h>
#include <custom_themes/lv_theme_rep_panel_dark.h>
#include "reppanel_machine.h"
#include "reppanel_helper.h"
#include "reppanel_request.h"
#include "reppanel.h"
#include "duet_status_json.h"

#define TAG     "Machine"

reprap_axes_t reprap_axes;
reprap_params_t reprap_params;

static lv_style_t not_homed_style, homed_style;
static char *cali_opt_map[] = {"True Bed Leveling", "Mesh Bed Leveling"};
static char *cali_opt_list = {"True Bed Leveling\nMesh Bed Leveling"};

#define AWAY_BTN    0
#define CLOSER_BTN  1

//#define USE_LIGHTNING                     // uncomment to add menu items for switching on/off a light/pin
#define LIGHTNING_CMD_ON "M42 P2 S1"
#define LIGHTNING_CMD_HALF "M42 P2 S0.5"
#define LIGHTNING_CMD_OFF "M42 P2 S0"

lv_obj_t *machine_page;
lv_obj_t *ddlist_cali_options;
lv_obj_t *btnm_height;
lv_obj_t *btn_closer;
lv_obj_t *btn_away;
lv_obj_t *cont_heigh_adj_diag, *label_z_pos_cali;
lv_obj_t *btn_home_all, *btn_home_y, *btn_home_x, *btn_home_z;
lv_obj_t *btn_power, *label_power;
lv_obj_t *btn_fan_off, *label_fan, *slider;

#ifdef USE_LIGHTNING
    lv_obj_t *btn_light_off, *btn_light_half, *btn_light_on;
#endif


static void home_all_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        reprap_send_gcode("G28");
    }
}

static void home_x_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        reprap_send_gcode("G28 X");
    }
}

static void home_y_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        reprap_send_gcode("G28 Y");
    }
}

static void home_z_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        reprap_send_gcode("G28 Z");
    }
}

#ifdef USE_LIGHTNING
static void _light_off_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        reprap_send_gcode(LIGHTNING_CMD_OFF);
    }
}

static void _light_half_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        reprap_send_gcode(LIGHTNING_CMD_HALF);
    }
}

static void _light_on_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        reprap_send_gcode(LIGHTNING_CMD_ON);
    }
}
#endif

static void power_toggle_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        reprap_send_gcode(reprap_params.power ? "M81" : "M80");
    }
}

static void fan_off_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        reprap_send_gcode("M106 S0");
    }
}

static void slider_event_cb(lv_obj_t * slider, lv_event_t event)
{
    if(event == LV_EVENT_RELEASED) {        
        static char buf[11]; /* max 10 bytes for number plus 1 null terminating byte */
        snprintf(buf, 11, "M106 S%.2f", (lv_slider_get_value(slider) / 100.));
        reprap_send_gcode(buf);
    }
}

static void next_height_adjust_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        if (reprap_send_gcode("M292")) {
            lv_obj_del(label_z_pos_cali);
            label_z_pos_cali = 0;               // otherwise crash in update_ui
            lv_obj_del(cont_heigh_adj_diag);
            seq_num_msgbox = 0;    // reset so we know msg GUI is not showing anymore - little dirty I know...
        }
    }
}

static void height_adjust_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_VALUE_CHANGED) {
        const char *amount = lv_btnm_get_active_btn_text(obj);
        char *dir;
        if (lv_btn_get_state(btn_closer) == LV_BTN_STATE_TGL_REL) {
            dir = "-";
        } else {
            dir = "";
        }
        ESP_LOGI(TAG, "Moving %s%s", dir, amount);
        char command[64];
        sprintf(command, "M120\nG91\nG1 Z%s%s F6000\nG90\nM121", dir, amount);
        reprap_send_gcode(command);
    }
}

static void away_closer_changed(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        if ((int) obj->user_data == CLOSER_BTN) {
            if (lv_btn_get_state(btn_closer) == LV_BTN_STATE_REL) {
                lv_btn_set_state(btn_away, LV_BTN_STATE_TGL_REL);
            } else {
                lv_btn_set_state(btn_away, LV_BTN_STATE_REL);
            }
        } else if ((int) obj->user_data == AWAY_BTN) {
            if (lv_btn_get_state(btn_away) == LV_BTN_STATE_REL) {
                lv_btn_set_state(btn_closer, LV_BTN_STATE_TGL_REL);
            } else {
                lv_btn_set_state(btn_closer, LV_BTN_STATE_REL);
            }
        }
    }
}

void show_height_adjust_dialog() {
    static const char *btns[] = {"5", "2.5", "0.5", "0.1", "0.05", "0.02", ""};
    cont_heigh_adj_diag = lv_cont_create(lv_layer_top(), NULL);
    static lv_style_t somestyle;
    lv_style_copy(&somestyle, lv_cont_get_style(cont_heigh_adj_diag, LV_CONT_STYLE_MAIN));
    somestyle.body.border.width = 1;
    somestyle.body.border.color = somestyle.body.main_color;
    somestyle.body.padding.left = LV_DPI / 6;
    somestyle.body.padding.right = LV_DPI / 6;
    somestyle.body.padding.top = LV_DPI / 12;
    somestyle.body.padding.bottom = LV_DPI / 12;
    somestyle.body.padding.inner = LV_DPI / 9;
    lv_cont_set_style(cont_heigh_adj_diag, LV_CONT_STYLE_MAIN, &somestyle);
    lv_cont_set_fit2(cont_heigh_adj_diag, LV_FIT_TIGHT, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_heigh_adj_diag, LV_LAYOUT_COL_M);

    btnm_height = lv_btnm_create(cont_heigh_adj_diag, NULL);

    lv_btnm_set_map(btnm_height, btns);
    lv_obj_set_event_cb(btnm_height, height_adjust_event);
    lv_obj_set_height(btnm_height, 65);
    lv_obj_set_width(btnm_height, 350);

    label_z_pos_cali = lv_label_create(cont_heigh_adj_diag, NULL);
    lv_label_set_text_fmt(label_z_pos_cali, "%.02f mm", reprap_axes.z);

    lv_obj_t *cont_closer_away = lv_cont_create(cont_heigh_adj_diag, NULL);
    lv_cont_set_layout(cont_closer_away, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(cont_closer_away, LV_FIT_TIGHT);

    const lv_style_t *panel_style = lv_cont_get_style(cont_closer_away, LV_CONT_STYLE_MAIN);
    static lv_style_t new_released_style;
    btn_closer = lv_btn_create(cont_closer_away, NULL);
    lv_style_copy(&new_released_style, lv_btn_get_style(btn_closer, LV_BTN_STYLE_REL));
    new_released_style.body.main_color = panel_style->body.main_color;
    new_released_style.body.grad_color = panel_style->body.grad_color;
    new_released_style.body.border.width = 0;
    new_released_style.body.padding.top = LV_DPI / 12;
    new_released_style.body.padding.bottom = LV_DPI / 12;

    LV_IMG_DECLARE(closer_icon);
    lv_btn_set_fit(btn_closer, LV_FIT_TIGHT);
    lv_btn_set_style(btn_closer, LV_BTN_STYLE_REL, &new_released_style);
    lv_obj_t *img1 = lv_img_create(btn_closer, NULL);
    lv_img_set_src(img1, &closer_icon);
    lv_btn_set_toggle(btn_closer, true);
    lv_btn_set_state(btn_closer, LV_BTN_STATE_TGL_REL);
    lv_obj_set_event_cb(btn_closer, away_closer_changed);
    lv_obj_set_user_data(btn_closer, (lv_obj_user_data_t) CLOSER_BTN);

    LV_IMG_DECLARE(away_icon);
    btn_away = lv_btn_create(cont_closer_away, btn_closer);
    lv_obj_t *img2 = lv_img_create(btn_away, NULL);
    lv_img_set_src(img2, &away_icon);
    lv_btn_set_state(btn_away, LV_BTN_STATE_REL);
    lv_obj_set_user_data(btn_away, (lv_obj_user_data_t) AWAY_BTN);

    lv_obj_t *spacer = lv_cont_create(cont_closer_away, NULL);
    lv_obj_set_width(spacer, 50);
    static lv_obj_t *btn_close;
    create_button(cont_closer_away, btn_close, "Next", next_height_adjust_event);
    lv_obj_align_origo(cont_heigh_adj_diag, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
}

static void start_cali_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        char val_txt_buff[50];
        lv_ddlist_get_selected_str(ddlist_cali_options, val_txt_buff, 50);
        if (strcmp(val_txt_buff, cali_opt_map[0]) == 0) {
            ESP_LOGI(TAG, "True Bed Leveling");
            reprap_send_gcode("G32");
        } else if (strcmp(val_txt_buff, cali_opt_map[1]) == 0) {
            ESP_LOGI(TAG, "Mesh Bed Leveling");
            reprap_send_gcode("G29");
        }
    }
}

void update_ui_machine() {
    portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mutex); // not sure this really helps?!
    if (label_z_pos_cali) lv_label_set_text_fmt(label_z_pos_cali, "%.02f mm", reprap_axes.z);
    portEXIT_CRITICAL(&mutex);
    
    if (btn_home_x) {
        if (reprap_axes.x_homed)
            lv_btn_set_style(btn_home_x, LV_BTN_STYLE_REL, &homed_style);
        else
            lv_btn_set_style(btn_home_x, LV_BTN_STYLE_REL, &not_homed_style);

        if (reprap_axes.y_homed)
            lv_btn_set_style(btn_home_y, LV_BTN_STYLE_REL, &homed_style);
        else
            lv_btn_set_style(btn_home_y, LV_BTN_STYLE_REL, &not_homed_style);

        if (reprap_axes.z_homed)
            lv_btn_set_style(btn_home_z, LV_BTN_STYLE_REL, &homed_style);
        else
            lv_btn_set_style(btn_home_z, LV_BTN_STYLE_REL, &not_homed_style);

        if (reprap_axes.x_homed && reprap_axes.y_homed && reprap_axes.z_homed)
            lv_btn_set_style(btn_home_all, LV_BTN_STYLE_REL, &homed_style);
        else
            lv_btn_set_style(btn_home_all, LV_BTN_STYLE_REL, &not_homed_style);
    }
    if (btn_power) {
        if (reprap_params.power) {
            lv_btn_set_style(btn_power, LV_BTN_STYLE_REL, &homed_style);
            lv_label_set_text(label_power, "On");
        } else {
            lv_btn_set_style(btn_power, LV_BTN_STYLE_REL, &not_homed_style);
            lv_label_set_text(label_power, "Off");
        }
    }
    if (label_fan) {
        lv_label_set_text_fmt(label_fan, " %u%% ", reprap_params.fan);
        lv_slider_set_value(slider, reprap_params.fan, LV_ANIM_ON);
    }
}

void draw_machine(lv_obj_t *parent_screen) {
    machine_page = lv_page_create(parent_screen, NULL);
    lv_obj_set_size(machine_page, lv_disp_get_hor_res(NULL),
                    lv_disp_get_ver_res(NULL) - (lv_obj_get_height(cont_header) + 5));
    lv_page_set_scrl_fit2(machine_page, LV_FIT_FILL, LV_FIT_FILL);
    lv_page_set_scrl_layout(machine_page, LV_LAYOUT_COL_M);

    lv_obj_t *home_cont = lv_cont_create(machine_page, NULL);
    lv_cont_set_layout(home_cont, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(home_cont, LV_FIT_TIGHT);
    lv_obj_t *label_home = lv_label_create(home_cont, NULL);
    lv_label_set_text(label_home, "Home:");
    btn_home_all = create_button(home_cont, btn_home_all, "  All Axis  ", home_all_event);
    btn_home_x = create_button(home_cont, btn_home_x, " X ", home_x_event);
    btn_home_y = create_button(home_cont, btn_home_y, " Y ", home_y_event);
    btn_home_z = create_button(home_cont, btn_home_z, " Z ", home_z_event);
    
    lv_style_copy(&not_homed_style, lv_btn_get_style(btn_home_x, LV_BTN_STYLE_REL));
    lv_style_copy(&homed_style, lv_btn_get_style(btn_home_x, LV_BTN_STYLE_REL));
    homed_style.body.main_color = REP_PANEL_DARK_ACCENT;
    homed_style.body.grad_color = REP_PANEL_DARK_ACCENT;
    homed_style.text.color = REP_PANEL_DARK;

    lv_obj_t *cont_cali = lv_cont_create(machine_page, NULL);
    lv_cont_set_fit(cont_cali, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_cali,  LV_LAYOUT_ROW_M);

    ddlist_cali_options = lv_ddlist_create(cont_cali, NULL);
    lv_ddlist_set_options(ddlist_cali_options, cali_opt_list);
    lv_ddlist_set_draw_arrow(ddlist_cali_options, true);
    lv_ddlist_set_fix_height(ddlist_cali_options, 110);
    lv_ddlist_set_sb_mode(ddlist_cali_options, LV_SB_MODE_AUTO);
    lv_ddlist_set_fix_width(ddlist_cali_options, 300);

    static lv_obj_t *do_cali_butn;
    create_button(cont_cali, do_cali_butn, "Start", start_cali_event);

    lv_obj_t *power_cont = lv_cont_create(machine_page, NULL);
    lv_cont_set_layout(power_cont,  LV_LAYOUT_ROW_M);
    lv_cont_set_fit(power_cont, LV_FIT_TIGHT);
    lv_obj_t *label_power_header = lv_label_create(power_cont, NULL);
    lv_label_set_text(label_power_header, "Power:");
    
    btn_power = lv_btn_create(power_cont, NULL);
    lv_btn_set_fit(btn_power, LV_FIT_TIGHT);
    lv_obj_set_event_cb(btn_power, power_toggle_event);
    lv_obj_align(btn_power, power_cont, LV_ALIGN_CENTER, 0, 0);
    label_power = lv_label_create(btn_power, NULL);
    lv_label_set_text(label_power, reprap_params.power ? "On" : "Off");

    #ifdef USE_LIGHTNING
    lv_obj_t *label_light = lv_label_create(power_cont, NULL);
    lv_label_set_text(label_light, "Light:");
    btn_light_on = create_button(power_cont, btn_light_on, "On", _light_on_event);
    btn_light_half = create_button(power_cont, btn_light_half, "50%", _light_half_event);
    btn_light_off = create_button(power_cont, btn_light_off, "Off", _light_off_event);
    #endif

    lv_obj_t *fan_cont = lv_cont_create(machine_page, NULL);
    lv_cont_set_layout(fan_cont,  LV_LAYOUT_ROW_M);
    lv_cont_set_fit(fan_cont, LV_FIT_TIGHT);
    lv_obj_t *label_fan_title = lv_label_create(fan_cont, NULL);
    lv_label_set_text(label_fan_title, "Fan:  ");    
    
    slider = lv_slider_create(fan_cont, NULL);
    lv_obj_set_width(slider, LV_DPI * 2);
    lv_obj_align(slider, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_event_cb(slider, slider_event_cb);
    lv_slider_set_range(slider, 0, 100);
    label_fan = lv_label_create(fan_cont, NULL);
    lv_label_set_text_fmt(label_fan, " %u%% ", reprap_params.fan);    
    btn_fan_off = create_button(fan_cont, btn_fan_off, " Off ", fan_off_event);

    update_ui_machine();
}