//
// Created by cyber on 15.03.20.
//


#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/lvgl.h>
#include <esp_log.h>
#include "reppanel.h"
#include "reppanel_request.h"

#define TAG "Macros"

lv_obj_t *macro_container;
lv_obj_t *macro_list;

static void _macro_clicked_event_handler(lv_obj_t * obj, lv_event_t event) {
    if(event == LV_EVENT_RELEASED) {
        ESP_LOGI(TAG, "Clicked: %s\n", lv_list_get_btn_text(obj));
    }
}

void draw_macro(lv_obj_t *parent_screen) {
    request_macros();
    macro_container = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(macro_container, LV_LAYOUT_ROW_T);
    lv_cont_set_fit2(macro_container, LV_FIT_FILL, LV_FIT_FILL);

    macro_list = lv_list_create(macro_container, NULL);
    lv_obj_set_size(macro_list, LV_HOR_RES-10, LV_VER_RES-50);
    lv_obj_align(macro_list, NULL, LV_ALIGN_IN_TOP_MID, 0, 50);

    lv_obj_t * list_btn;
    for (int i = 0; reprap_macro_names[i] != NULL; i++) {
        list_btn = lv_list_add_btn(macro_list, LV_SYMBOL_FILE, reprap_macro_names[i]);
        lv_obj_set_event_cb(list_btn, _macro_clicked_event_handler);
    }
}

#include "reppanel_macros.h"
