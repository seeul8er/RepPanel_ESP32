//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef LVGL_REPPANEL_CONSOLE_H
#define LVGL_REPPANEL_CONSOLE_H

#include <lvgl/src/lv_core/lv_obj.h>

#define MAX_CONSOLE_ENTRY_COUNT     16
#define MAX_LEN_COMMAND             32
#define MAX_LEN_RESPONSE            128

enum console_msg_type {
    CONSOLE_TYPE_EMPTY, CONSOLE_TYPE_INFO, CONSOLE_TYPE_WARN, CONSOLE_TYPE_REPPANEL
};

void add_console_hist_entry(char *command, enum console_msg_type type);

void draw_console(lv_obj_t *parent_screen);

void update_entries_ui();

typedef struct {
    char command[MAX_LEN_COMMAND];
//    char response[MAX_LEN_RESPONSE];
    enum console_msg_type type;
} console_entry_t;

extern console_entry_t console_enties[MAX_CONSOLE_ENTRY_COUNT];

#endif //LVGL_REPPANEL_CONSOLE_H
