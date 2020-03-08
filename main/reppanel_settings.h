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


static char wifi_ssid[32];
static char wifi_pass[64];

static char rep_addr[200];
static char rep_pass[64];

static int temp_unit = 0;   // 0=Celsius, 1=Fahrenheit

static char *tool_names_map[] = {"E0", "E1", "E2", "E3", "E4", "E5", "E6"};
static char *filament_names = {
        "TEQStonePLA white\nINNOFIL PLA orange\nABS\nPETG FANCY\nUBS\nPEEK\nUltimaker PEAK\nBASF Wonderstuff"};

static char *bed_tmps_active = "0°C\n40°C\n60°C\n100°C";
static char *bed_tmps_standby = "0°C\n40°C\n60°C\n100°C";
static char *extruder_tmps_active = "0°C\n100°C\n160°C\n200°C";
static char *extruder_tmps_standby = "0°C\n100°C\n160°C\n200°C";

static int toolstates[MAX_NUM_TOOLS];       // 0=off, 1=standby, 2=active, 3=fault - Storage for incoming data


void write_settings_to_nvs();
void read_settings_nvs();

#endif //LVGL_REPPANEL_SETTINGS_H
