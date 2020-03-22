//
// Created by cyber on 21.03.20.
//

#include <stdio.h>
#include <lvgl/lvgl.h>
#include <stdlib.h>
#include <custom_themes/lv_theme_rep_panel_light.h>
#include <esp_log.h>
#include "reppanel.h"
#include "reppanel_request.h"

#define TAG             "JobSelect"
#define CANCEL_BTN_TXT  "#ffffff Cancel#"
#define DELETE_BTN_TXT  "#c43145 "LV_SYMBOL_TRASH" Delete#"
#define SIM_BTN_TXT     LV_SYMBOL_EYE_OPEN" Simulate"
#define PRINT_BTN_TXT   REP_PANEL_DARK_ACCENT_STR " " LV_SYMBOL_PLAY" Print#"

file_tree_elem_t reprap_jobs[MAX_NUM_JOBS];

lv_obj_t *jobs_container;
lv_obj_t *jobs_list;
lv_obj_t *msg_box1;
lv_obj_t *msg_box2;
lv_obj_t *msg_box3;

reprap_job_t *edit_file;

static void print_file_handler(lv_obj_t * obj, lv_event_t event) {
    if(event == LV_EVENT_RELEASED) {
        if (strcmp(lv_mbox_get_active_btn_text(msg_box3), "Yes") == 0) {
            ESP_LOGI(TAG, "Printing %s", edit_file->name);
            // TODO: Print file
            lv_obj_del_async(msg_box3);
            display_jobstatus();
        } else {
            lv_obj_del_async(msg_box3);
        }
    }
}

static void delete_file_handler(lv_obj_t * obj, lv_event_t event) {
    if(event == LV_EVENT_RELEASED) {
        if (strcmp(lv_mbox_get_active_btn_text(msg_box2), "Yes") == 0) {
            ESP_LOGI(TAG, "Deleting %s", edit_file->name);
            // TODO: Delete file
            lv_obj_del_async(msg_box2);
            lv_obj_del_async(msg_box1);
        } else {
            lv_obj_del_async(msg_box2);
        }
    }
}

static void job_action_handler(lv_obj_t *obj, lv_event_t event) {
    if(event == LV_EVENT_CLICKED) {
        if (strcmp(lv_mbox_get_active_btn_text(msg_box1), CANCEL_BTN_TXT) == 0) {
            ESP_LOGI(TAG, "Close window. No action");
            lv_obj_del_async(msg_box1);
        } else if (strcmp(lv_mbox_get_active_btn_text(msg_box1), DELETE_BTN_TXT) == 0) {
            static const char * btns[] ={"Yes", "No", ""};
            msg_box2 = lv_mbox_create(lv_layer_top(), NULL);
            lv_mbox_set_text(msg_box2, "Do you really want to delete this file?");
            lv_mbox_add_btns(msg_box2, btns);
            lv_obj_set_event_cb(msg_box2, delete_file_handler);
            lv_obj_set_width(msg_box2, lv_disp_get_hor_res(NULL) - 20);
            lv_obj_align(msg_box2, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
        } else if (strcmp(lv_mbox_get_active_btn_text(msg_box1), PRINT_BTN_TXT) == 0) {
            ESP_LOGI(TAG, "Print %s", edit_file->name);
            // TODO: Print file
            lv_obj_del_async(msg_box1);
            display_jobstatus();
        } else if (strcmp(lv_mbox_get_active_btn_text(msg_box1), SIM_BTN_TXT) == 0) {
            ESP_LOGI(TAG, "Simulate %s", edit_file->name);
            char tmp_txt[strlen(edit_file->dir) + strlen(edit_file->name) + 10];
            sprintf(tmp_txt, "M37 P\"%s/%s\"", edit_file->dir, edit_file->name);
            reprap_send_gcode(tmp_txt);
            lv_obj_del_async(msg_box1);
            display_jobstatus();
        }
    }
}

static void _job_clicked_event_handler(lv_obj_t *obj, lv_event_t event) {
    int selected_indx = lv_list_get_btn_index(jobs_list, obj);
    if(event == LV_EVENT_SHORT_CLICKED) {
        if (reprap_jobs[selected_indx].type == TREE_FILE_ELEM) {
            static const char * btns[] ={"Yes", "No", ""};
            msg_box3 = lv_mbox_create(lv_layer_top(), NULL);
            char msg[100];
            edit_file = (reprap_job_t*) reprap_jobs[selected_indx].element;
            sprintf(msg, "Do you want to print %s?", lv_list_get_btn_text(obj));
            lv_mbox_set_text(msg_box3, msg);
            lv_mbox_add_btns(msg_box3, btns);
            lv_obj_set_event_cb(msg_box3, print_file_handler);
            lv_obj_set_width(msg_box3, lv_disp_get_hor_res(NULL) - 20);
            lv_obj_align(msg_box3, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
        } else if (reprap_jobs[selected_indx].type == TREE_FOLDER_ELEM) {
            // TODO: Get folder content
            // TODO: Display folder content
        }
    } else if (event == LV_EVENT_LONG_PRESSED && reprap_jobs[selected_indx].type == TREE_FILE_ELEM) {
        edit_file = (reprap_job_t*) reprap_jobs[selected_indx].element;
        static const char * btns[] ={SIM_BTN_TXT, PRINT_BTN_TXT, DELETE_BTN_TXT, CANCEL_BTN_TXT, ""};
        msg_box1 = lv_mbox_create(lv_layer_top(), NULL);
        lv_mbox_set_text(msg_box1, "Select action");
        lv_mbox_add_btns(msg_box1, btns);
        lv_mbox_set_recolor(msg_box1, true);
        lv_obj_set_width(msg_box1, lv_disp_get_hor_res(NULL) - 30);
        lv_obj_set_event_cb(msg_box1, job_action_handler);
        lv_obj_align(msg_box1, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
    }
}


void draw_jobselect(lv_obj_t *parent_screen) {
    request_jobs();

    jobs_container = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(jobs_container, LV_LAYOUT_COL_M);
    lv_cont_set_fit(jobs_container, LV_FIT_FILL);

    lv_obj_t *label_info = lv_label_create(jobs_container, NULL);
    lv_label_set_text(label_info, "Select print job");

    jobs_list = lv_list_create(jobs_container, NULL);
    lv_obj_set_size(jobs_list, LV_HOR_RES-10, lv_disp_get_ver_res(NULL) - 100);

    for (int i = 0; reprap_jobs[i].element != NULL; i++) {
        lv_obj_t *list_btn;
        if (reprap_jobs[i].type == TREE_FOLDER_ELEM) {
            list_btn = lv_list_add_btn(jobs_list, LV_SYMBOL_DIRECTORY, ((reprap_job_t*) reprap_jobs[i].element)->name);
        } else {
            list_btn = lv_list_add_btn(jobs_list, LV_SYMBOL_FILE, ((reprap_job_t*) reprap_jobs[i].element)->name);
        }
        lv_obj_set_event_cb(list_btn, _job_clicked_event_handler);
    }
}