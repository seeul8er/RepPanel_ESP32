//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/src/lv_objx/lv_cont.h>
#include <lvgl/src/lv_objx/lv_page.h>
#include <lvgl/lvgl.h>
#include <custom_themes/lv_theme_rep_panel_dark.h>
#include <esp_log.h>
#include "reppanel.h"
#include "reppanel_request.h"
#include "rrf_objects.h"

#define TAG "JobStatus"

float reprap_job_percent;
double reprap_job_first_layer_height = 0;
double reprap_job_layer_height = 0;

lv_obj_t *cont_percent;
lv_obj_t *label_job_progress_percent;
lv_obj_t *label_job_elapsed_time;
lv_obj_t *label_job_remaining_time;
lv_obj_t *label_job_layer_status;
lv_obj_t *label_job_filename;

static lv_style_t style_button_job_pause;
lv_obj_t *button_job_pause;
lv_obj_t *button_job_resume;
lv_obj_t *button_job_stop;

void update_print_job_status_ui() {
    if (label_job_progress_percent && cont_percent) {
        lv_label_set_text_fmt(label_job_progress_percent, "%.0f", reprap_job_percent);
        if (reprap_job_percent < 10) {
            lv_obj_align(label_job_progress_percent, cont_percent, LV_ALIGN_CENTER, 15, 0);
        } else {
            lv_obj_align(label_job_progress_percent, cont_percent, LV_ALIGN_CENTER, -15, 0);
        }
    }

    if (reprap_job_first_layer_height > 0 && reprap_job_layer_height > 0 && reprap_model.reprap_job.file.height > 0 &&
        label_job_layer_status) {
        if (reprap_model.reprap_job.file.numLayers == 0) {
            int total_layer_cnt = (int) (((reprap_model.reprap_job.file.height - reprap_job_first_layer_height) / reprap_job_layer_height) +
                                         1);
            lv_label_set_text_fmt(label_job_layer_status, "%i/%i", reprap_model.reprap_job.layer, total_layer_cnt);
        } else {
            lv_label_set_text_fmt(label_job_layer_status, "%i/%i", reprap_model.reprap_job.layer, reprap_model.reprap_job.file.numLayers);
        }
    } else if (label_job_layer_status) {
        lv_label_set_text(label_job_layer_status, "");
    }

    int job_dur_h = (int) (reprap_model.reprap_job.duration / (60 * 60));
    int job_dur_min = (int) ((reprap_model.reprap_job.duration - (job_dur_h * 60 * 60)) / 60);
    if (job_dur_h > 0 && label_job_elapsed_time) {
        lv_label_set_text_fmt(label_job_elapsed_time, "%ih %imin", job_dur_h, job_dur_min);
    } else if (job_dur_min > 0 && label_job_elapsed_time) {
        lv_label_set_text_fmt(label_job_elapsed_time, "%imin", job_dur_min);
    } else if (label_job_elapsed_time && reprap_model.reprap_job.duration > 0) {
        lv_label_set_text_fmt(label_job_elapsed_time, "%.0fs", reprap_model.reprap_job.duration);
    }
    double sim_time_left, file_time_left;
    if (reprap_model.api_level < 1) {
        sim_time_left = (reprap_model.reprap_job.file.simulatedTime - reprap_model.reprap_job.duration);     // time left according to simulation
        file_time_left = (reprap_model.reprap_job.file.printTime - reprap_model.reprap_job.duration);   // time left according to file info
    } else {
        sim_time_left = reprap_model.reprap_job.timesLeft.simulation;
        file_time_left = reprap_model.reprap_job.timesLeft.slicer;
    }
    if (((int) sim_time_left) > 0) {
        int job_dur_sim_h = (int) (sim_time_left / (60 * 60));
        int job_dur_sim_min = (int) ((sim_time_left - (job_dur_sim_h * 60 * 60)) / 60);
        if (job_dur_sim_h > 0 && label_job_remaining_time) {
            lv_label_set_text_fmt(label_job_remaining_time, "%ih %imin", job_dur_sim_h, job_dur_sim_min);
        } else if (label_job_remaining_time) {
            lv_label_set_text_fmt(label_job_remaining_time, "%imin", job_dur_sim_min);
        }
    } else if (((int) file_time_left) > 0) {
        int job_dur_file_h = (int) (file_time_left / (60 * 60));
        int job_dur_file_min = (int) ((file_time_left - (job_dur_file_h * 60 * 60)) / 60);
        if (job_dur_file_h > 0 && label_job_remaining_time) {
            lv_label_set_text_fmt(label_job_remaining_time, "%ih %imin", job_dur_file_h, job_dur_file_min);
        } else if (job_dur_file_min > 0 && label_job_remaining_time) {
            lv_label_set_text_fmt(label_job_remaining_time, "%imin", job_dur_file_min);
        } else if (label_job_remaining_time) {
            lv_label_set_text_fmt(label_job_remaining_time, "%.0fs", file_time_left);
        }
    }

    if (label_job_filename) {
        // only update when changed. Otherwise label will not scroll
        char *last = strrchr(reprap_model.reprap_job.file.fileName, '/'); // remove first part of the path
        if (last != NULL && strcmp(lv_label_get_text(label_job_filename), last+1) != 0) {
            lv_label_set_text(label_job_filename, last+1);
        }
    }

    if (button_job_pause) {
        if (job_paused) {
            lv_obj_set_hidden(button_job_pause, true);
            lv_obj_set_hidden(button_job_resume, false);
            lv_obj_set_hidden(button_job_stop, false);
        } else {
            lv_obj_set_hidden(button_job_pause, false);
            lv_obj_set_hidden(button_job_resume, true);
            lv_obj_set_hidden(button_job_stop, true);
        }
    }
}

void resume_job_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Resuming print job");
        reprap_send_gcode("M24");
    }
}

void stop_job_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Stopping print job");
        reprap_send_gcode("M0 H1");
    }
}

void pause_job_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Pausing print job");
        reprap_send_gcode("M25");
    }
}


void draw_jobstatus(lv_obj_t *parent_screen) {
    lv_obj_t *jobstatus_page = lv_page_create(parent_screen, NULL);
    lv_obj_set_size(jobstatus_page, lv_disp_get_hor_res(NULL),
                    lv_disp_get_ver_res(NULL) - (lv_obj_get_height(cont_header) + 5));
    lv_page_set_scrl_fit(jobstatus_page, LV_FIT_FLOOD);
    lv_page_set_scrl_layout(jobstatus_page, LV_LAYOUT_OFF);
    static lv_style_t style_jobstatus_page;
    lv_style_copy(&style_jobstatus_page, lv_page_get_style(jobstatus_page, LV_PAGE_STYLE_SCRL));
    style_jobstatus_page.body.padding.top = 0;
    style_jobstatus_page.body.padding.inner = 0;
    lv_page_set_style(jobstatus_page, LV_PAGE_STYLE_SCRL, &style_jobstatus_page);

    lv_obj_t *cont_main = lv_cont_create(jobstatus_page, NULL);
    lv_cont_set_fit(cont_main, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_main, LV_LAYOUT_ROW_M);
    static lv_style_t style_cont_main;
    lv_style_copy(&style_cont_main, lv_cont_get_style(cont_main, LV_CONT_STYLE_MAIN));
    style_cont_main.body.padding.top = 0;
    lv_cont_set_style(cont_main, LV_CONT_STYLE_MAIN, &style_cont_main);

    cont_percent = lv_cont_create(cont_main, NULL);
    lv_obj_set_width(cont_percent, 200);
    lv_obj_set_height(cont_percent, 120);
    lv_cont_set_layout(cont_percent, LV_LAYOUT_OFF);

    lv_obj_t *cont_jobdetails = lv_cont_create(cont_main, NULL);
    lv_cont_set_fit(cont_jobdetails, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_jobdetails, LV_LAYOUT_COL_L);
    static lv_style_t style_cont_jobdetails;
    lv_style_copy(&style_cont_jobdetails, lv_cont_get_style(cont_jobdetails, LV_CONT_STYLE_MAIN));
    style_cont_jobdetails.body.padding.left = 0;
    lv_cont_set_style(cont_jobdetails, LV_CONT_STYLE_MAIN, &style_cont_jobdetails);

    label_job_progress_percent = lv_label_create(cont_percent, NULL);
    static lv_style_t style_status_label;
    lv_style_copy(&style_status_label, &lv_style_plain);
    style_status_label.text.color = REP_PANEL_DARK_ACCENT;
    style_status_label.text.font = &reppanel_font_roboto_thin_numeric_160;
    lv_obj_set_style(label_job_progress_percent, &style_status_label);
    lv_label_set_align(label_job_progress_percent, LV_LABEL_ALIGN_RIGHT);

    lv_obj_t *label_percent = lv_label_create(cont_percent, NULL);
    static lv_style_t style_label_percent;
    lv_style_copy(&style_label_percent, &lv_style_plain);
    style_label_percent.text.color = REP_PANEL_DARK_TEXT;
    style_label_percent.text.font = &reppanel_font_roboto_regular_percent_40;
    lv_obj_set_style(label_percent, &style_label_percent);
    lv_label_set_text(label_percent, "%");

    lv_obj_t *cont_elapsed = lv_cont_create(cont_jobdetails, NULL);
    lv_cont_set_fit(cont_elapsed, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_elapsed, LV_LAYOUT_ROW_M);
    static lv_style_t style_cont_elapsed;
    lv_style_copy(&style_cont_elapsed, lv_cont_get_style(cont_elapsed, LV_CONT_STYLE_MAIN));
    style_cont_elapsed.body.padding.left = 0;
    lv_cont_set_style(cont_elapsed, LV_CONT_STYLE_MAIN, &style_cont_elapsed);
    lv_obj_t *img_elapsed = lv_img_create(cont_elapsed, NULL);
    LV_IMG_DECLARE(elapsed_time);
    lv_img_set_src(img_elapsed, &elapsed_time);
    label_job_elapsed_time = lv_label_create(cont_elapsed, NULL);
    static lv_style_t style_job_info_label;
    lv_style_copy(&style_job_info_label, &lv_style_plain);
    style_job_info_label.text.color = REP_PANEL_DARK_TEXT;
    style_job_info_label.text.font = &lv_font_roboto_28;

    lv_obj_set_style(label_job_elapsed_time, &style_job_info_label);

    lv_obj_t *cont_remain = lv_cont_create(cont_jobdetails, NULL);
    lv_cont_set_fit(cont_remain, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_remain, LV_LAYOUT_ROW_M);
    lv_obj_t *img_remain = lv_img_create(cont_remain, NULL);
    LV_IMG_DECLARE(remaining_clock);
    lv_img_set_src(img_remain, &remaining_clock);
    label_job_remaining_time = lv_label_create(cont_remain, NULL);
    lv_obj_set_style(label_job_remaining_time, &style_job_info_label);
    lv_label_set_text(label_job_remaining_time, "");

    lv_obj_t *cont_layer_status = lv_cont_create(cont_jobdetails, cont_remain);
    lv_obj_t *img_layers = lv_img_create(cont_layer_status, NULL);
    LV_IMG_DECLARE(layers);
    lv_img_set_src(img_layers, &layers);
    label_job_layer_status = lv_label_create(cont_layer_status, NULL);
    lv_obj_set_style(label_job_layer_status, &style_job_info_label);

    label_job_filename = lv_label_create(jobstatus_page, NULL);
    static lv_style_t style_label_job_filename;
    lv_style_copy(&style_label_job_filename, lv_label_get_style(label_job_filename, LV_LABEL_STYLE_MAIN));
    style_label_job_filename.text.color = REP_PANEL_DARK_TEXT;
    style_label_job_filename.text.font = &reppanel_font_roboto_light_26;
    lv_obj_set_style(label_job_filename, &style_label_job_filename);
    lv_label_set_long_mode(label_job_filename, LV_LABEL_LONG_CROP);     // circ scroll makes it run out of heap?!
    lv_obj_set_width(label_job_filename, lv_disp_get_hor_res(NULL) - 150);

    LV_IMG_DECLARE(pause);
    lv_style_copy(&style_button_job_pause, &lv_style_plain);
    style_button_job_pause.image.color = LV_COLOR_BLACK;
    style_button_job_pause.image.intense = LV_OPA_50;
    style_button_job_pause.text.color = lv_color_hex3(0xaaa);

    button_job_pause = lv_imgbtn_create(jobstatus_page, NULL);
    lv_imgbtn_set_src(button_job_pause, LV_BTN_STATE_REL, &pause);
    lv_imgbtn_set_src(button_job_pause, LV_BTN_STATE_PR, &pause);
    lv_imgbtn_set_src(button_job_pause, LV_BTN_STATE_TGL_REL, &pause);
    lv_imgbtn_set_src(button_job_pause, LV_BTN_STATE_TGL_PR, &pause);
    lv_imgbtn_set_style(button_job_pause, LV_BTN_STATE_PR, &style_button_job_pause);
    lv_imgbtn_set_style(button_job_pause, LV_BTN_STATE_TGL_PR, &style_button_job_pause);
    lv_imgbtn_set_toggle(button_job_pause, true);
    lv_obj_set_event_cb(button_job_pause, pause_job_event);

    lv_obj_align(button_job_pause, jobstatus_page, LV_ALIGN_IN_BOTTOM_RIGHT, -32, -30);
    lv_obj_align(label_job_filename, jobstatus_page, LV_ALIGN_IN_BOTTOM_LEFT, 15, -30);
    lv_obj_align_origo(cont_main, jobstatus_page, LV_ALIGN_CENTER, -40, -50);
    lv_obj_align(label_percent, cont_percent, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
    lv_obj_align(label_job_progress_percent, cont_percent, LV_ALIGN_CENTER, -85, 0);

    // hidden from the start
    static lv_style_t style_button_job_resume;
    lv_style_copy(&style_button_job_resume, &lv_style_plain);
    style_button_job_resume.image.color = REP_PANEL_DARK_GREEN;
    style_button_job_resume.image.intense = LV_OPA_100;

    // hidden from the start
    LV_IMG_DECLARE(play);
    button_job_resume = lv_imgbtn_create(jobstatus_page, NULL);
    lv_imgbtn_set_src(button_job_resume, LV_BTN_STATE_REL, &play);
    lv_imgbtn_set_src(button_job_resume, LV_BTN_STATE_PR, &play);
    lv_imgbtn_set_src(button_job_resume, LV_BTN_STATE_TGL_REL, &play);
    lv_imgbtn_set_src(button_job_resume, LV_BTN_STATE_TGL_PR, &play);
    lv_imgbtn_set_style(button_job_resume, LV_BTN_STATE_REL, &style_button_job_resume);
    lv_imgbtn_set_style(button_job_resume, LV_BTN_STATE_PR, &style_button_job_pause);
    lv_imgbtn_set_style(button_job_resume, LV_BTN_STATE_TGL_PR, &style_button_job_pause);
    lv_imgbtn_set_toggle(button_job_resume, true);
    lv_obj_set_event_cb(button_job_resume, resume_job_event);

    static lv_style_t style_button_job_stop;
    lv_style_copy(&style_button_job_stop, &lv_style_plain);
    style_button_job_stop.image.color = REP_PANEL_DARK_RED;
    style_button_job_stop.image.intense = LV_OPA_100;

    // hidden from the start
    LV_IMG_DECLARE(stop);
    button_job_stop = lv_imgbtn_create(jobstatus_page, NULL);
    lv_imgbtn_set_src(button_job_stop, LV_BTN_STATE_REL, &stop);
    lv_imgbtn_set_src(button_job_stop, LV_BTN_STATE_PR, &stop);
    lv_imgbtn_set_src(button_job_stop, LV_BTN_STATE_TGL_REL, &stop);
    lv_imgbtn_set_src(button_job_stop, LV_BTN_STATE_TGL_PR, &stop);
    lv_imgbtn_set_style(button_job_stop, LV_BTN_STATE_REL, &style_button_job_stop);
    lv_imgbtn_set_style(button_job_stop, LV_BTN_STATE_PR, &style_button_job_pause);
    lv_imgbtn_set_style(button_job_stop, LV_BTN_STATE_TGL_PR, &style_button_job_pause);
    lv_imgbtn_set_toggle(button_job_stop, true);
    lv_obj_set_event_cb(button_job_stop, stop_job_event);

    // hidden from the start
    lv_obj_align(button_job_resume, jobstatus_page, LV_ALIGN_IN_BOTTOM_RIGHT, -32, -30);
    lv_obj_align(button_job_stop, jobstatus_page, LV_ALIGN_IN_BOTTOM_RIGHT, -84, -30);

    request_fileinfo(NULL);
    update_print_job_status_ui();
}