//
// Created by cyber on 16.02.20.
//

#ifndef LVGL_REPPANEL_H
#define LVGL_REPPANEL_H

#include <stdint.h>

#include "reppanel_helper.h"
#include "esp32_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

// set as user data to ddlists to identify within common event
#define DDLIST_BED_TMP_ACTIVE 0
#define DDLIST_BED_TMP_STANDBY 1
#define DDLIST_TOOL_TMP_ACTIVE 2
#define DDLIST_TOOL_TMP_STANDBY 3

#define VERSION_STR             "v0.1.0"

extern uint8_t reppanel_conn_status;               // 0=no connection, 1=connected wifi, 2=disconnected wifi, 3=reconnecting wifi,
                                            // 4=working UART

extern lv_obj_t *process_scr;               // screen for the process settings
extern lv_obj_t *mainmenu_scr;              // screen for the main_menue

extern lv_obj_t *cont_main;
extern lv_obj_t *label_status;
extern lv_obj_t *label_bed_temp;
extern lv_obj_t *label_tool_temp;
// extern lv_obj_t *label_progress_percent;
extern lv_obj_t *main_menu_button;
extern lv_obj_t *console_button;
extern lv_obj_t *label_extruder_name;
extern lv_obj_t *ddlist_tool_temp_active;
extern lv_obj_t *ddlist_tool_temp_standby;
extern lv_obj_t *ddlist_bed_temp_active;
extern lv_obj_t *ddlist_bed_temp_standby;
extern lv_obj_t *button_tool_filament;
extern lv_obj_t *label_sig_strength;
extern lv_obj_t *ta_wifi_pass;
extern lv_obj_t *ta_ssid;
extern lv_obj_t *ta_printer_addr;
extern lv_obj_t *ta_reprap_pass;
extern lv_obj_t *label_connection_status;

void rep_panel_ui_create();
void update_rep_panel_conn_status();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif //LVGL_REPPANEL_H
