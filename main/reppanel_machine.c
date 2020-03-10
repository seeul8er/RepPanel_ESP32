//
// Created by cyber on 01.03.20.
//

#include <stdio.h>
#include <lvgl/src/lv_objx/lv_page.h>
#include <lvgl/src/lv_objx/lv_ddlist.h>
#include "reppanel_machine.h"
#include "reppanel_helper.h"

static char *cali_opt_map[] = {"True Bed Leveling", "Mesh Bed Leveling"};
static char *cali_opt_list = {"True Bed Leveling\nMesh Bed Leveling"};

lv_obj_t *machine_page;
lv_obj_t *ddlist_cali_options;

static void cali_opt_selected_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_VALUE_CHANGED) {
        char val_txt_buff[50];
        lv_ddlist_get_selected_str(obj, val_txt_buff, 50);
        if (strcmp(val_txt_buff, cali_opt_map[0]) == 0) {
            printf("true bed leveling\n");
            // TODO: Implement true bed leveling call
        } else if (strcmp(val_txt_buff, cali_opt_map[1]) == 0) {
            // TODO: Implement mesh bed leveling call
            printf("mesh bed leveling\n");
        }
    }
}

static void home_all_event(lv_obj_t *obj, lv_event_t event) {
    // TODO: Implement Home All
}

static void home_x_event(lv_obj_t *obj, lv_event_t event) {
    // TODO: Implement Home X
}

static void home_y_event(lv_obj_t *obj, lv_event_t event) {
    // TODO: Implement Home Y
}

static void home_z_event(lv_obj_t *obj, lv_event_t event) {
    // TODO: Implement Home Z
}

void draw_machine(lv_obj_t *parent_screen) {
    machine_page = lv_page_create(parent_screen, NULL);
    lv_obj_set_size(machine_page, lv_disp_get_hor_res(NULL), 250);
    lv_page_set_scrl_fit2(machine_page, LV_FIT_FILL, LV_FIT_FILL);
    lv_page_set_scrl_layout(machine_page, LV_LAYOUT_COL_M);

    ddlist_cali_options = lv_ddlist_create(machine_page, NULL);
    lv_ddlist_set_options(ddlist_cali_options, cali_opt_list);
    lv_ddlist_set_draw_arrow(ddlist_cali_options, true);
    lv_ddlist_set_fix_height(ddlist_cali_options, 110);
    lv_ddlist_set_sb_mode(ddlist_cali_options, LV_SB_MODE_AUTO);
    lv_obj_align(ddlist_cali_options, machine_page, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_obj_set_event_cb(ddlist_cali_options, cali_opt_selected_event);

    lv_obj_t *home_cont = lv_cont_create(machine_page, NULL);
    lv_cont_set_layout(home_cont, LV_LAYOUT_ROW_M);
    lv_cont_set_fit(home_cont, LV_FIT_TIGHT);
    lv_obj_t *label_home = lv_label_create(home_cont, NULL);
    lv_label_set_text(label_home, "Home:");
    static lv_obj_t *btn_home_all, *btn_home_y, *btn_home_x, *btn_home_z;
    create_button(home_cont, btn_home_all, "  All Axis  ", home_all_event);
    create_button(home_cont, btn_home_x, " X ", home_x_event);
    create_button(home_cont, btn_home_y, " Y ", home_y_event);
    create_button(home_cont, btn_home_z, " Z ", home_z_event);
}