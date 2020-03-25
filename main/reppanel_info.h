//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef LVGL_REPPANEL_INFO_H
#define LVGL_REPPANEL_INFO_H

#include <lvgl/src/lv_core/lv_obj.h>

extern lv_obj_t *ta_wifi_pass;
extern lv_obj_t *ta_ssid;
extern lv_obj_t *ta_printer_addr;
extern lv_obj_t *ta_reprap_pass;

void draw_info(lv_obj_t *parent_screen);

#endif //LVGL_REPPANEL_INFO_H
