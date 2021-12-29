//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <lvgl/src/lv_core/lv_obj.h>

#ifndef LVGL_REPPANEL_MACHINE_H
#define LVGL_REPPANEL_MACHINE_H

void update_ui_machine();
void show_reprap_dialog(char *title, char *msg, const uint8_t *mode, bool show_height_adjust);
void draw_machine(lv_obj_t *parent_screen);

#endif //LVGL_REPPANEL_MACHINE_H
