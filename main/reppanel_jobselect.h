//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef LVGL_REPPANEL_JOBSELECT_H
#define LVGL_REPPANEL_JOBSELECT_H

#include <lvgl/src/lv_core/lv_obj.h>
#include "rrf_objects.h"

void draw_jobselect(lv_obj_t *parent_screen);

void update_job_list_ui();

void update_file_info_dialog_ui(reprap_model_t *_reprap_model);

#endif //LVGL_REPPANEL_JOBSELECT_H
