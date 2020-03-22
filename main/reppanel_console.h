//
// Created by cyber on 21.03.20.
//

#ifndef LVGL_REPPANEL_CONSOLE_H
#define LVGL_REPPANEL_CONSOLE_H

#include <lvgl/src/lv_core/lv_obj.h>

#define CONSOLE_TYPE_INFO   0
#define CONSOLE_TYPE_WARN   1
#define CONSOLE_TYPE_REPPANEL   2

#define MAX_CONSOLE_ENTRY_COUNT   20

void draw_console(lv_obj_t *parent_screen);

typedef struct {
    char *command;
    char *response;
    int type;
} console_entry_t;

extern console_entry_t console_enties[MAX_CONSOLE_ENTRY_COUNT];

#endif //LVGL_REPPANEL_CONSOLE_H
