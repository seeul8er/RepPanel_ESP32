//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef LVGL_REPPANEL_H
#define LVGL_REPPANEL_H

#include <stdint.h>

#include "reppanel_helper.h"
#include "esp32_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_REPRAP_STATUS_LEN   15
#define MAX_PREPANEL_TEMP_LEN   8

#define BTN_BED_TMP_ACTIVE 0
#define BTN_BED_TMP_STANDBY 1
#define BTN_TOOL_TMP_ACTIVE 2
#define BTN_TOOL_TMP_STANDBY 3

#define REPPANEL_NO_CONNECTION      0
#define REPPANEL_WIFI_CONNECTED     1
#define REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED     2
#define REPPANEL_WIFI_DISCONNECTED  3
#define REPPANEL_WIFI_RECONNECTING  4
#define REPPANEL_UART_CONNECTED     5

#define VERSION_STR             "v0.1.0"

#define NUM_TEMPS_BUFF      15
#define MAX_FILA_NAME_LEN   64
#define MAX_TOOL_NAME_LEN   32
#define MAX_LEN_STR_FILAMENT_LIST   512*3
#define MAX_NUM_MACROS      32
#define MAX_NUM_JOBS        MAX_NUM_MACROS

#define TREE_EMPTY_ELEM     -1
#define TREE_FOLDER_ELEM    0
#define TREE_FILE_ELEM      1

extern uint8_t reppanel_conn_status;    // See REPPANEL_NO_CONNECTION, REPPANEL_WIFI_CONNECTED, etc.

extern lv_obj_t *process_scr;               // screen for the process settings
extern lv_obj_t *mainmenu_scr;              // screen for the main_menue
extern lv_obj_t *cont_header;

extern lv_obj_t *label_status;
extern lv_obj_t *label_chamber_temp;
extern lv_obj_t *label_bed_temp;
extern lv_obj_t *label_tool_temp;
// extern lv_obj_t *label_progress_percent;
extern lv_obj_t *main_menu_button;
extern lv_obj_t *console_button;
extern lv_obj_t *label_extruder_name;

extern lv_obj_t *button_tool_filament;
extern lv_obj_t *ddlist_selected_filament;
extern lv_obj_t *label_sig_strength;
extern lv_obj_t *label_connection_status;

// Temp variable for writing to label. Contains current temp + °C or °F
extern char reppanel_status[MAX_REPRAP_STATUS_LEN];

extern int reprap_chamber_temp_curr_pos;
extern double reprap_chamber_temp_buff[NUM_TEMPS_BUFF];

// predefined by d2wc config. Temps the heaters can be set to
extern double reprap_babysteps_amount;
extern double reprap_extruder_amounts[NUM_TEMPS_BUFF];
extern double reprap_extruder_feedrates[NUM_TEMPS_BUFF];
extern double reprap_move_feedrate;
extern double reprap_mcu_temp;
extern double reprap_job_percent;
extern int reprap_job_file_pos;
extern double reprap_job_duration;
extern int reprap_job_curr_layer;
extern int reprap_job_time_file;
extern int reprap_job_time_sim;
extern double reprap_job_first_layer_height;
extern double reprap_job_layer_height;
extern double reprap_job_height;
extern char current_job_name[MAX_FILA_NAME_LEN];
extern char reprap_firmware_name[100];
extern char reprap_firmware_version[5];


typedef struct {
    int number;
    char name[MAX_TOOL_NAME_LEN];
    int fans;
    char filament[MAX_FILA_NAME_LEN];
    int heater_indx;                    // only support one heater per tool for now
    double temp_buff[NUM_TEMPS_BUFF];   // Temp buffer contains temperature history of heaters
    int temp_hist_curr_pos;             // Pointers to current position within the temp buffer
    double active_temp;
    double standby_temp;
} reprap_tool_t;

typedef struct {
    double temp_buff[NUM_TEMPS_BUFF];   // Temp buffer contains temperature history of heater
    int temp_hist_curr_pos;             // Pointers to current position within the temp buffer
    int heater_indx;
    double active_temp;
    double standby_temp;
} reprap_bed_t;

typedef struct {
    double temps_standby[NUM_TEMPS_BUFF];
    double temps_active[NUM_TEMPS_BUFF];
} reprap_tool_poss_temps_t;

typedef struct {
    double temps_standby[NUM_TEMPS_BUFF];
    double temps_active[NUM_TEMPS_BUFF];
} reprap_bed_poss_temps_t;

typedef struct {
    char *name;
    char *last_mod;
    char *dir;
    char *generator;        // slicer engine
    int size;
    double height;
    double layer_height;
    int print_time;
    int sim_print_time;
} reprap_job_t;

typedef struct {
    char *name;
    char *last_mod;
    char *dir;
    int size;
} reprap_macro_t;

typedef struct {
    void *element;  // reprap_macro_t or reprap_job_t
    int type;       // TREE_FOLDER_ELEM, TREE_FILE_ELEM
} file_tree_elem_t;

extern file_tree_elem_t reprap_jobs[MAX_NUM_JOBS];
extern file_tree_elem_t reprap_macros[MAX_NUM_MACROS];

// pos 0 is bed temp, rest are tool heaters
extern int heater_states[MAX_NUM_TOOLS];       // 0=off, 1=standby, 2=active, 3=fault - Storage for incoming data
extern int num_heaters;     // max is MAX_NUM_TOOLS
extern int num_tools;     // max is MAX_NUM_TOOLS
extern int current_visible_tool_indx;   // current indx of tool where temp data is displayed on process screen

extern reprap_tool_t reprap_tools[MAX_NUM_TOOLS];
extern reprap_bed_t reprap_bed;
extern reprap_tool_poss_temps_t reprap_tool_poss_temps;
extern reprap_bed_poss_temps_t reprap_bed_poss_temps;

extern char filament_names[MAX_LEN_STR_FILAMENT_LIST];

extern bool job_running;

void rep_panel_ui_create();

void update_rep_panel_conn_status();

void display_jobstatus();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif //LVGL_REPPANEL_H
