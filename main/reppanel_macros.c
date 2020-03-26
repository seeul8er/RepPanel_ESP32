//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/lvgl.h>
#include <esp_log.h>
#include "reppanel.h"
#include "reppanel_request.h"

#define TAG "Macros"

file_tree_elem_t reprap_macros[MAX_NUM_JOBS];

lv_obj_t *macro_container;
lv_obj_t *macro_list;
lv_obj_t *msg_box3;
lv_obj_t *preloader;
reprap_macro_t *edit_macro;

static void exe_macro_file_handler(lv_obj_t * obj, lv_event_t event) {
    if(event == LV_EVENT_RELEASED) {
        if (strcmp(lv_mbox_get_active_btn_text(msg_box3), "Yes") == 0) {
            ESP_LOGI(TAG, "Running file %s", lv_list_get_btn_text(obj));
            char tmp_txt[strlen(edit_macro->dir) + strlen(edit_macro->name) + 10];
            sprintf(tmp_txt, "M98 P\"%s/%s\"", edit_macro->dir, edit_macro->name);
            reprap_send_gcode(tmp_txt);
            lv_obj_del_async(msg_box3);
        } else {
            lv_obj_del_async(msg_box3);
        }
    }
}

static void _macro_clicked_event_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_RELEASED) {
        int selected_indx = lv_list_get_btn_index(macro_list, obj);
        if (reprap_macros[selected_indx].type == TREE_FILE_ELEM) {
            static const char * btns[] ={"Yes", "No", ""};
            msg_box3 = lv_mbox_create(lv_layer_top(), NULL);
            char msg[100];
            edit_macro = (reprap_macro_t*) reprap_jobs[selected_indx].element;
            sprintf(msg, "Do you want to execute %s?", lv_list_get_btn_text(obj));
            lv_mbox_set_text(msg_box3, msg);
            lv_mbox_add_btns(msg_box3, btns);
            lv_obj_set_event_cb(msg_box3, exe_macro_file_handler);
            lv_obj_set_width(msg_box3, lv_disp_get_hor_res(NULL) - 20);
            lv_obj_align(msg_box3, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
        } else if (reprap_macros[selected_indx].type == TREE_FOLDER_ELEM) {
            ESP_LOGI(TAG, "Clicked folder %s", lv_list_get_btn_text(obj));
            // TODO: List folder elements
        }
    }
}

void update_macro_list_ui() {
    lv_obj_del(preloader);
    lv_obj_t *list_btn;
    macro_list = lv_list_create(macro_container, NULL);
    lv_obj_set_size(macro_list, LV_HOR_RES - 10, lv_disp_get_ver_res(NULL) - (lv_obj_get_height(cont_header) + 5));
    for (int i = 0; reprap_macros[i].element != NULL; i++) {
        if (reprap_macros[i].type == TREE_FOLDER_ELEM)
            list_btn = lv_list_add_btn(macro_list, LV_SYMBOL_DIRECTORY,
                                       ((reprap_macro_t *) reprap_macros[i].element)->name);
        else
            list_btn = lv_list_add_btn(macro_list, LV_SYMBOL_FILE, ((reprap_macro_t *) reprap_macros[i].element)->name);
        lv_obj_set_event_cb(list_btn, _macro_clicked_event_handler);
    }
}

void draw_macro(lv_obj_t *parent_screen) {
    macro_container = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(macro_container, LV_LAYOUT_CENTER);
    lv_cont_set_fit2(macro_container, LV_FIT_FILL, LV_FIT_TIGHT);

    preloader = lv_preload_create(macro_container, NULL);
    lv_obj_set_size(preloader, 75, 75);

    request_macros_async();
}

#include "reppanel_macros.h"
