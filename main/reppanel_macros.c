//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/lvgl.h>
#include <esp_log.h>
#include "reppanel.h"
#include "reppanel_request.h"

#define TAG "Macros"

#define MACRO_ROOT_DIR  "0:/macros"
#define MACRO_EMPTY ""
#define BACK_TXT    "Back"

file_tree_elem_t reprap_macros[MAX_NUM_MACROS_DIR];

lv_obj_t *macro_list;
lv_obj_t *msg_box3;
lv_obj_t *preloader;
reprap_macro_t *edit_macro;
char *edit_path;
char parent_dir_macros[MAX_LEN_DIRNAME + 1];

static void exe_macro_file_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        if (strcmp(lv_mbox_get_active_btn_text(msg_box3), "Yes") == 0) {
            ESP_LOGI(TAG, "Running file %s", lv_list_get_btn_text(obj));
            char tmp_txt[strlen(edit_path) + strlen(edit_macro->name) + 10];
            sprintf(tmp_txt, "M98 P\"%s/%s\"", edit_path, edit_macro->name);
            reprap_send_gcode(tmp_txt);
            lv_obj_del_async(msg_box3);
        } else {
            lv_obj_del_async(msg_box3);
        }
    }
}

static void macro_clicked_event_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        int selected_indx = lv_list_get_btn_index(macro_list, obj);
        // check if back button exists
        if (strcmp(reprap_macros[selected_indx].dir, MACRO_ROOT_DIR) != 0) {
            if (selected_indx == 0) {
                // back button was pressed
                ESP_LOGI(TAG, "Going back to parent %s", parent_dir_macros);
                if (!preloader)
                    preloader = lv_preload_create(lv_layer_top(), NULL);
                lv_obj_set_size(preloader, 75, 75);
                lv_obj_align_origo(preloader, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
                request_macros(parent_dir_macros);
                return;
            } else {
                // no back button pressed
                // decrease index to match with reprap_macros array indexing
                selected_indx--;
            }
        }
        edit_macro = (reprap_macro_t *) reprap_macros[selected_indx].element;
        if (edit_macro == NULL)
            return;
        edit_path = reprap_macros[selected_indx].dir;
        if (reprap_macros[selected_indx].type == TREE_FILE_ELEM) {
            static const char *btns[] = {"Yes", "No", ""};
            msg_box3 = lv_mbox_create(lv_layer_top(), NULL);
            char msg[100];
            sprintf(msg, "Do you want to execute %s?", edit_macro->name);
            lv_mbox_set_text(msg_box3, msg);
            lv_mbox_add_btns(msg_box3, btns);
            lv_obj_set_event_cb(msg_box3, exe_macro_file_handler);
            lv_obj_set_width(msg_box3, lv_disp_get_hor_res(NULL) - 20);
            lv_obj_align(msg_box3, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
        } else if (reprap_macros[selected_indx].type == TREE_FOLDER_ELEM) {
            ESP_LOGI(TAG, "Clicked folder %s (index %i)", edit_macro->name, selected_indx);
            if (!preloader)
                preloader = lv_preload_create(lv_layer_top(), NULL);
            lv_obj_set_size(preloader, 75, 75);
            lv_obj_align_origo(preloader, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
            static char tmp_txt[MAX_LEN_DIRNAME + MAX_LEN_FILENAME + 1];
            sprintf(tmp_txt, "%s/%s", reprap_macros[selected_indx].dir, edit_macro->name);
            request_macros(tmp_txt);
        }
    }
}

void update_macro_list_ui() {
    lv_obj_del(preloader);
    if (macro_list) {
        lv_list_clean(macro_list);
    } else {
        return;
    }

    // Add back button in case we are not in root directory
    if (strcmp(reprap_macros[0].dir, MACRO_ROOT_DIR) != 0) {
        lv_obj_t *back_btn;
        back_btn = lv_list_add_btn(macro_list, LV_SYMBOL_LEFT, BACK_TXT);
        lv_obj_set_event_cb(back_btn, macro_clicked_event_handler);
        // update parent dir
        strcpy(parent_dir_macros, reprap_macros[0].dir);
        char *pch;
        pch = strrchr(parent_dir_macros, '/');
        parent_dir_macros[pch - parent_dir_macros] = '\0';
    } else {
        strcpy(parent_dir_macros, MACRO_EMPTY);
    }
    for (int i = 0; reprap_macros[i].element != NULL; i++) {
        lv_obj_t *list_btn;
        if (reprap_macros[i].type == TREE_FOLDER_ELEM)
            list_btn = lv_list_add_btn(macro_list, LV_SYMBOL_DIRECTORY,
                                       ((reprap_macro_t *) reprap_macros[i].element)->name);
        else
            list_btn = lv_list_add_btn(macro_list, LV_SYMBOL_FILE, ((reprap_macro_t *) reprap_macros[i].element)->name);
        lv_obj_set_event_cb(list_btn, macro_clicked_event_handler);
    }
}

void draw_macro(lv_obj_t *parent_screen) {
    strcpy(parent_dir_macros, MACRO_EMPTY);
    lv_obj_t *macro_container = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(macro_container, LV_LAYOUT_CENTER);
    lv_cont_set_fit2(macro_container, LV_FIT_FILL, LV_FIT_TIGHT);

    preloader = lv_preload_create(macro_container, NULL);
    lv_obj_set_size(preloader, 75, 75);

    macro_list = lv_list_create(macro_container, NULL);
    lv_obj_set_size(macro_list, LV_HOR_RES - 10, lv_disp_get_ver_res(NULL) - (lv_obj_get_height(cont_header) + 5));

    request_macros(MACRO_ROOT_DIR);
}

#include "reppanel_macros.h"
