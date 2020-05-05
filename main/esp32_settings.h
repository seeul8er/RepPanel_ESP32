//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

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
#define MAX_REP_ADDR_LEN    128
#define MAX_REP_PASS_LEN    64

extern char wifi_ssid[MAX_SSID_LEN];
extern char wifi_pass[MAX_REP_PASS_LEN];
extern char rep_addr[MAX_REP_ADDR_LEN];
extern char rep_pass[MAX_REP_PASS_LEN];
extern int temp_unit;   // 0=Celsius, 1=Fahrenheit

void write_settings_to_nvs();

void read_settings_nvs();

char get_temp_unit();

#endif //LVGL_REPPANEL_SETTINGS_H
