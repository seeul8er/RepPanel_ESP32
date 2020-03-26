//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef LVGL_REPPANEL_CONSOLE_H
#define LVGL_REPPANEL_CONSOLE_H

#include <lvgl/src/lv_core/lv_obj.h>

#define MAX_CONSOLE_ENTRY_COUNT   20

enum {CONSOLE_TYPE_INFO, CONSOLE_TYPE_WARN, CONSOLE_TYPE_REPPANEL};

void draw_console(lv_obj_t *parent_screen);

typedef struct {
    char *command;
    char *response;
    int type;
} console_entry_t;

extern console_entry_t console_enties[MAX_CONSOLE_ENTRY_COUNT];

#endif //LVGL_REPPANEL_CONSOLE_H
