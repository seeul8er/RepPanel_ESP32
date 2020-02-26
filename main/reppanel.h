//
// Created by cyber on 23.02.20.
//

#ifndef REPPANEL_ESP32_REPPANEL_H
#define REPPANEL_ESP32_REPPANEL_H

#include <lvgl/src/lv_core/lv_obj.h>

extern lv_obj_t *label_status;
extern lv_obj_t *label_bed_temp;
extern lv_obj_t *label_tool1_temp;
extern lv_obj_t *label_progress_percent;

void rep_panel_ui_create();

#endif //REPPANEL_ESP32_REPPANEL_H
