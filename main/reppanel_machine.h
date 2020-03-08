//
// Created by cyber on 01.03.20.
//

#include <lvgl/src/lv_core/lv_obj.h>

#ifndef LVGL_REPPANEL_MACHINE_H
#define LVGL_REPPANEL_MACHINE_H

static char *cali_opt_map[] = {"True Bed Leveling", "Mesh Bed Leveling"};
static char *cali_opt_list = {"True Bed Leveling\nMesh Bed Leveling"};

void draw_machine(lv_obj_t *parent_screen);

#endif //LVGL_REPPANEL_MACHINE_H
