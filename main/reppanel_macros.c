//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/lvgl.h>
#include <esp_log.h>
#include "reppanel.h"
#include "reppanel_request.h"

#define TAG "Macros"

file_tree_elem_t reprap_macros[MAX_NUM_MACROS];

lv_obj_t *macro_container;
lv_obj_t *macro_list;
lv_obj_t *msg_box3;
lv_obj_t *preloader;
reprap_macro_t *edit_macro;
char *prev_dir;

static void _exe_macro_file_handler(lv_obj_t * obj, lv_event_t event) {
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
        edit_macro = (reprap_macro_t*) reprap_macros[selected_indx].element;
        static char tmp_txt[128];
        if (reprap_macros[selected_indx].type == TREE_FILE_ELEM) {
            static const char * btns[] ={"Yes", "No", ""};
            msg_box3 = lv_mbox_create(lv_layer_top(), NULL);
            char msg[100];
            sprintf(msg, "Do you want to execute %s?", edit_macro->name);
            lv_mbox_set_text(msg_box3, msg);
            lv_mbox_add_btns(msg_box3, btns);
            lv_obj_set_event_cb(msg_box3, _exe_macro_file_handler);
            lv_obj_set_width(msg_box3, lv_disp_get_hor_res(NULL) - 20);
            lv_obj_align(msg_box3, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
        } else if (reprap_macros[selected_indx].type == TREE_FOLDER_ELEM) {
            ESP_LOGI(TAG, "Clicked folder %s", edit_macro->name);
            preloader = lv_preload_create(lv_layer_top(), NULL);
            lv_obj_set_size(preloader, 75, 75);
            lv_obj_align_origo(preloader, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
            sprintf(tmp_txt, "%s/%s&first=0", edit_macro->dir, edit_macro->name);
            prev_dir = edit_macro->dir;
            request_macros_async(tmp_txt);
        }
    }
}

void _nav_back_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_RELEASED) {
        static char tmp_txt[128];
        sprintf(tmp_txt, "%s&first=0", prev_dir);
        request_macros_async(tmp_txt);
    }
}

void update_macro_list_ui() {
    if (preloader) {
        lv_obj_del(preloader);
        lv_obj_t *list_btn;
        if (macro_list == NULL) {
            macro_list = lv_list_create(macro_container, NULL);
            lv_obj_set_size(macro_list, LV_HOR_RES - 10, lv_disp_get_ver_res(NULL) - (lv_obj_get_height(cont_header) + 5));
        } else {
            lv_list_clean(macro_list);
        }
        for (int i = 0; reprap_macros[i].element != NULL; i++) {
            if (reprap_macros[i].type == TREE_FOLDER_ELEM)
                list_btn = lv_list_add_btn(macro_list, LV_SYMBOL_DIRECTORY,
                                           ((reprap_macro_t *) reprap_macros[i].element)->name);
            else
                list_btn = lv_list_add_btn(macro_list, LV_SYMBOL_FILE, ((reprap_macro_t *) reprap_macros[i].element)->name);
            lv_obj_set_event_cb(list_btn, _macro_clicked_event_handler);
        }
    }
    static lv_obj_t *back_bnt;
    //create_button(lv_layer_top(), back_bnt, LV_SYMBOL_LEFT " Back", _nav_back_event);
    //lv_obj_align(back_bnt, lv_layer_top(), LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
}

void draw_macro(lv_obj_t *parent_screen) {
    macro_container = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(macro_container, LV_LAYOUT_CENTER);
    lv_cont_set_fit2(macro_container, LV_FIT_FILL, LV_FIT_TIGHT);

    preloader = lv_preload_create(macro_container, NULL);
    lv_obj_set_size(preloader, 75, 75);

    request_macros_async("0:/macros&first=0");
}

#include "reppanel_macros.h"
