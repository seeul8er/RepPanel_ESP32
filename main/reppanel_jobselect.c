//
// Created by cyber on 21.03.20.
//

#include <stdio.h>
#include <lvgl/lvgl.h>
#include <stdlib.h>
#include <custom_themes/lv_theme_rep_panel_light.h>
#include "reppanel.h"

#define CANCEL_BTN_TXT  "#ffffff Cancel#"
#define DELETE_BTN_TXT  "#c43145 "LV_SYMBOL_TRASH" Delete#"
#define SIM_BTN_TXT     LV_SYMBOL_EYE_OPEN" Simulate"
#define PRINT_BTN_TXT   REP_PANEL_DARK_ACCENT_STR " " LV_SYMBOL_PLAY" Print#"

lv_obj_t *jobs_container;
lv_obj_t *jobs_list;
lv_obj_t *msg_box1;
lv_obj_t *msg_box2;
lv_obj_t *msg_box3;

char edit_file[MAX_FILA_NAME_LEN];

static void print_file_handler(lv_obj_t * obj, lv_event_t event) {
    if(event == LV_EVENT_RELEASED) {
        if (strcmp(lv_mbox_get_active_btn_text(msg_box3), "Yes") == 0) {
            printf("Printing %s\n", edit_file);
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
            printf("Deleting %s\n", edit_file);
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
            printf("Close window. No action\n");
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
            printf("Print %s\n", edit_file);
            // TODO: Print file
            lv_obj_del_async(msg_box1);
            display_jobstatus();
        } else if (strcmp(lv_mbox_get_active_btn_text(msg_box1), SIM_BTN_TXT) == 0) {
            printf("Simulate %s\n", edit_file);
            // TODO: Simulate file
            lv_obj_del_async(msg_box1);
        }
    }
}

static void _job_clicked_event_handler(lv_obj_t *obj, lv_event_t event) {
    if(event == LV_EVENT_SHORT_CLICKED) {
        static const char * btns[] ={"Yes", "No", ""};
        msg_box3 = lv_mbox_create(lv_layer_top(), NULL);
        char msg[100];
        strcpy(edit_file, lv_list_get_btn_text(obj));
        // edit_file = lv_list_get_btn_text(obj);
        sprintf(msg, "Do you want to print %s?", lv_list_get_btn_text(obj));
        lv_mbox_set_text(msg_box3, msg);
        lv_mbox_add_btns(msg_box3, btns);
        lv_obj_set_event_cb(msg_box3, print_file_handler);
        lv_obj_set_width(msg_box3, lv_disp_get_hor_res(NULL) - 20);
        lv_obj_align(msg_box3, lv_layer_top(), LV_ALIGN_CENTER, 0, 0);
    } else if (event == LV_EVENT_LONG_PRESSED) {
        strcpy(edit_file, lv_list_get_btn_text(obj));
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
    char *reprap_job_names[10];
    for (int i = 0; i < 10; i++) {
        reprap_job_names[i] = NULL;
        free(reprap_job_names[i]);
    }
    reprap_job_names[0] = malloc(strlen("Job 1") + 1);
    strcpy(reprap_job_names[0], "Job 1");
    reprap_job_names[1] = malloc(strlen("Job 2") + 1);
    strcpy(reprap_job_names[1], "Job 2");
    reprap_job_names[2] = malloc(strlen("Job 3") + 1);
    strcpy(reprap_job_names[2], "Job 3");
    reprap_job_names[3] = malloc(strlen("Job 4") + 1);
    strcpy(reprap_job_names[3], "Job 4");
    reprap_job_names[4] = malloc(strlen("Job 5") + 1);
    strcpy(reprap_job_names[4], "Job 5");

    jobs_container = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(jobs_container, LV_LAYOUT_COL_M);
    lv_cont_set_fit(jobs_container, LV_FIT_FILL);

    lv_obj_t *label_info = lv_label_create(jobs_container, NULL);
    lv_label_set_text(label_info, "Select print job");

    jobs_list = lv_list_create(jobs_container, NULL);
    lv_obj_set_size(jobs_list, LV_HOR_RES-10, lv_disp_get_ver_res(NULL) - 100);

    lv_obj_t * list_btn;
    for (int i = 0; reprap_job_names[i] != NULL; i++) {
        list_btn = lv_list_add_btn(jobs_list, LV_SYMBOL_FILE, reprap_job_names[i]);
        lv_obj_set_event_cb(list_btn, _job_clicked_event_handler);
    }
}