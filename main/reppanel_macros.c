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

static void _macro_clicked_event_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_RELEASED) {
        int selected_indx = lv_list_get_btn_index(macro_list, obj);
        if (reprap_macros[selected_indx].type == TREE_FILE_ELEM) {
            ESP_LOGI(TAG, "Clicked file %s", lv_list_get_btn_text(obj));
            char tmp_txt[strlen(((reprap_macro_t *) reprap_macros[selected_indx].element)->dir) +
                         strlen(((reprap_macro_t *) reprap_macros[selected_indx].element)->name) + 10];
            sprintf(tmp_txt, "M98 P\"%s/%s\"", ((reprap_macro_t *) reprap_macros[selected_indx].element)->dir,
                    ((reprap_macro_t *) reprap_macros[selected_indx].element)->name);
            reprap_send_gcode(tmp_txt);
        } else if (reprap_macros[selected_indx].type == TREE_FOLDER_ELEM) {
            ESP_LOGI(TAG, "Clicked folder %s", lv_list_get_btn_text(obj));
            // TODO: List folder elements
        }
    }
}

void draw_macro(lv_obj_t *parent_screen) {
    macro_container = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(macro_container, LV_LAYOUT_ROW_T);
    lv_cont_set_fit2(macro_container, LV_FIT_FILL, LV_FIT_FILL);

    macro_list = lv_list_create(macro_container, NULL);
    lv_obj_set_size(macro_list, LV_HOR_RES - 10, lv_disp_get_ver_res(NULL) - (lv_obj_get_height(cont_header) + 5));
    lv_obj_align(macro_list, NULL, LV_ALIGN_IN_TOP_MID, 0, 50);

    request_macros();
    lv_obj_t *list_btn;
    for (int i = 0; reprap_macros[i].element != NULL; i++) {
        if (reprap_macros[i].type == TREE_FOLDER_ELEM)
            list_btn = lv_list_add_btn(macro_list, LV_SYMBOL_DIRECTORY,
                                       ((reprap_macro_t *) reprap_macros[i].element)->name);
        else
            list_btn = lv_list_add_btn(macro_list, LV_SYMBOL_FILE, ((reprap_macro_t *) reprap_macros[i].element)->name);
        lv_obj_set_event_cb(list_btn, _macro_clicked_event_handler);
    }
}

#include "reppanel_macros.h"
