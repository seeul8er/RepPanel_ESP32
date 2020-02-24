//
// Created by cyber on 23.02.20.
//

#include <lvgl/src/lv_core/lv_style.h>
#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/src/lv_objx/lv_label.h>
#include <lvgl/lvgl.h>
#include "custom_theme/lv_theme_rep_panel_light.h"
#include "reppanel.h"

void draw_process();

/**********************
 *  STATIC VARIABLES
 **********************/

void rep_panel_ui_create() {
    lv_coord_t hres = lv_disp_get_hor_res(NULL);
    lv_coord_t vres = lv_disp_get_ver_res(NULL);

    lv_theme_t * th = lv_theme_reppanel_light_init(210, NULL);     //Set a HUE value and a Font for the Night Theme
    lv_theme_set_current(th);
    draw_process();
}

void draw_process() {
    lv_obj_t *cont = lv_cont_create(lv_scr_act(), NULL);
    lv_cont_set_fit(cont, LV_FIT_FILL);

    label_bed_temp = lv_label_create(cont, NULL);
    static lv_style_t style_label;
    lv_style_copy(&style_label, &lv_style_plain);
    style_label.text.color = REP_PANEL_HIGHLIGHT;
    style_label.text.font = &lv_font_roboto_28;
    lv_label_set_text(label_bed_temp, "65°C");
    lv_obj_set_style(label_bed_temp, &style_label);

}
