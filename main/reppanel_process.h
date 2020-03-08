//
// Created by cyber on 01.03.20.
//

#ifndef LVGL_REPPANEL_PROCESS_H
#define LVGL_REPPANEL_PROCESS_H

#include "reppanel_settings.h"

void draw_process(lv_obj_t *parent_screen);
void update_heater_status(const int states[MAX_NUM_TOOLS], int num_heaters);

#endif //LVGL_REPPANEL_PROCESS_H
