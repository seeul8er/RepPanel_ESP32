//
// Created by cyber on 29.02.20.
//

#ifndef LVGL_REPPANEL_SETTINGS_H
#define LVGL_REPPANEL_SETTINGS_H

#define MAX_NUM_TOOLS   5

#define REPPANEL_NVS            "settings"
#define NVS_KEY_WIFI_SSID       "ssid"
#define NVS_KEY_WIFI_PASS       "wifi_pass"
#define NVS_KEY_REPRAP_ADDR     "rep_addr"
#define NVS_KEY_REPRAP_PASS     "rep_pass"

#define MAX_SSID_LEN        32
#define MAX_WIFI_PASS_LEN   64
#define MAX_REP_ADDR_LEN    200
#define MAX_REP_PASS_LEN    64


extern char wifi_ssid[32];
extern char wifi_pass[64];

extern char rep_addr[200];
extern char rep_pass[64];

extern int temp_unit;   // 0=Celsius, 1=Fahrenheit

extern char *tool_names_map[];
extern char *filament_names;

extern char *bed_tmps_active;
extern char *bed_tmps_standby;
extern char *extruder_tmps_active;
extern char *extruder_tmps_standby;

extern int toolstates[MAX_NUM_TOOLS];       // 0=off, 1=standby, 2=active, 3=fault - Storage for incoming data


void write_settings_to_nvs();
void read_settings_nvs();
char get_temp_unit();

#endif //LVGL_REPPANEL_SETTINGS_H
