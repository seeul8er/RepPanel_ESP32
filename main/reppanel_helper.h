//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef LVGL_REPPANEL_HELPER_H
#define LVGL_REPPANEL_HELPER_H

#include <lvgl/src/lv_core/lv_obj.h>

LV_FONT_DECLARE(reppanel_font_roboto_light_36)
LV_FONT_DECLARE(reppanel_font_roboto_bold_22)
LV_FONT_DECLARE(reppanel_font_roboto_regular_22)

char *url_encode(unsigned char *s, char *enc);

void init_reprap_buffers();

lv_obj_t *create_button(lv_obj_t *parent, lv_obj_t *button_pnt, char *text, void *event_handler);

void reppanel_disp_msg(char *msg_txt);

void _duet_show_dialog(char *title, char *msg);

void RepPanelLogE(char *tag, char *msg);

void RepPanelLogW(char *tag, char *msg);

void RepPanelLogI(char *tag, char *msg);

void RepPanelLogD(char *tag, char *msg);

void RepPanelLogV(char *tag, char *msg);

#endif //LVGL_REPPANEL_HELPER_H
