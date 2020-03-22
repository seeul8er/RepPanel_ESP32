//
// Created by cyber on 01.03.20.
//

#ifndef LVGL_REPPANEL_PROCESS_H
#define LVGL_REPPANEL_PROCESS_H

#include "esp32_settings.h"

void draw_process(lv_obj_t *parent_screen);
void update_heater_status(const int states[MAX_NUM_TOOLS], int num_heaters);
void update_bed_temps_ui();
void update_current_tool_temps_ui();

extern lv_obj_t *label_bed_temp_active;
extern lv_obj_t *label_bed_temp_standby;
extern lv_obj_t *label_tool_temp_active;
extern lv_obj_t *label_tool_temp_standby;

#endif //LVGL_REPPANEL_PROCESS_H
