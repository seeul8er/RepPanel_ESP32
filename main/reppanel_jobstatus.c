//
// Created by cyber on 20.03.20.
//

#include <lvgl/src/lv_core/lv_obj.h>
#include <lvgl/src/lv_objx/lv_cont.h>
#include <lvgl/src/lv_objx/lv_page.h>
#include <lvgl/lvgl.h>
#include <custom_themes/lv_theme_rep_panel_light.h>

lv_obj_t *jobstatus_page;
lv_obj_t *label_job_progress_percent;
lv_obj_t *label_job_elapsed_time;
lv_obj_t *label_job_remaining_time;
lv_obj_t *label_job_layer_status;
lv_obj_t *label_job_filename;

lv_obj_t *button_job_pause;
lv_obj_t *button_job_resume;
lv_obj_t *button_job_stop;


void draw_jobstatus(lv_obj_t *parent_screen) {
    jobstatus_page = lv_page_create(parent_screen, NULL);
    lv_obj_set_size(jobstatus_page, lv_disp_get_hor_res(NULL), 270);
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

    lv_obj_t *cont_percent = lv_cont_create(cont_main, NULL);
    lv_cont_set_fit(cont_percent, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_percent, LV_LAYOUT_ROW_B);
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
    lv_label_set_text(label_job_progress_percent, "99");

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
    lv_obj_t * img_elapsed = lv_img_create(cont_elapsed, NULL);
    LV_IMG_DECLARE(elapsed_time);
    lv_img_set_src(img_elapsed, &elapsed_time);
    label_job_elapsed_time = lv_label_create(cont_elapsed, NULL);
    static lv_style_t style_job_info_label;
    lv_style_copy(&style_job_info_label, &lv_style_plain);
    style_job_info_label.text.color = REP_PANEL_DARK_TEXT;
    style_job_info_label.text.font = &lv_font_roboto_28;

    lv_obj_set_style(label_job_elapsed_time, &style_job_info_label);
    lv_label_set_text(label_job_elapsed_time, "13h 46min");

    lv_obj_t *cont_remain = lv_cont_create(cont_jobdetails, NULL);
    lv_cont_set_fit(cont_remain, LV_FIT_TIGHT);
    lv_cont_set_layout(cont_remain, LV_LAYOUT_ROW_M);
    lv_obj_t * img_remain = lv_img_create(cont_remain, NULL);
    LV_IMG_DECLARE(remaining_clock);
    lv_img_set_src(img_remain, &remaining_clock);
    label_job_remaining_time = lv_label_create(cont_remain, NULL);
    lv_obj_set_style(label_job_remaining_time, &style_job_info_label);
    lv_label_set_text(label_job_remaining_time, "25h 64min");

    lv_obj_t *cont_layer_status = lv_cont_create(cont_jobdetails, cont_remain);
    lv_obj_t * img_layers = lv_img_create(cont_layer_status, NULL);
    LV_IMG_DECLARE(layers);
    lv_img_set_src(img_layers, &layers);
    label_job_layer_status = lv_label_create(cont_layer_status, NULL);
    lv_obj_set_style(label_job_layer_status, &style_job_info_label);
    lv_label_set_text(label_job_layer_status, "1789/6513");

    label_job_filename = lv_label_create(jobstatus_page, NULL);
    static lv_style_t style_label_job_filename;
    lv_style_copy(&style_label_job_filename, &lv_style_plain);
    style_label_job_filename.text.color = REP_PANEL_DARK_TEXT;
    style_label_job_filename.text.font = &reppanel_font_roboto_light_26;
    lv_obj_set_style(label_job_filename, &style_label_job_filename);
    lv_label_set_long_mode(label_job_filename, LV_LABEL_LONG_SROLL_CIRC);     /*Circular scroll*/
    lv_obj_set_width(label_job_filename, lv_disp_get_hor_res(NULL) - 150);
    lv_label_set_text(label_job_filename, "Benchy3D_FFF_v2 --------------- fgsdgfsdfgsdfgsd");

    LV_IMG_DECLARE(pause);
    static lv_style_t style_button_job_pause;
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

    lv_obj_align(button_job_pause, jobstatus_page, LV_ALIGN_IN_BOTTOM_RIGHT, -32, -30);
    lv_obj_align(label_job_filename, jobstatus_page, LV_ALIGN_IN_BOTTOM_LEFT, 15, -30);
    lv_obj_align_origo(cont_main, jobstatus_page, LV_ALIGN_IN_TOP_MID, 0, lv_obj_get_height(cont_main)-50);

}