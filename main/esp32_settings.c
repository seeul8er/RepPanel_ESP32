//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <nvs.h>
#include <string.h>
#include <esp_log.h>
#include "esp32_settings.h"
#include "reppanel.h"

#define TAG "RepPanelSettings"

char wifi_ssid[MAX_SSID_LEN];
char wifi_pass[MAX_WIFI_PASS_LEN];

char rep_addr[MAX_REP_ADDR_LEN];
char rep_pass[MAX_REP_PASS_LEN];

int temp_unit = 0;   // 0=Celsius, 1=Fahrenheit

char filament_names[MAX_LEN_STR_FILAMENT_LIST] = {"Not set\nNot set"};

char get_temp_unit() {
    if (temp_unit == 0) {
        return 'C';
    } else {
        return 'F';
    }
}

void print_settings() {
    ESP_LOGI(TAG, "Wifi SSID: %s", wifi_ssid);
    ESP_LOGI(TAG, "Wifi password: %s", wifi_pass);
    ESP_LOGI(TAG, "RepRap addr: %s", rep_addr);
    ESP_LOGI(TAG, "RepRap password: %s", rep_pass);
}

void init_settings() {
    memccpy(wifi_ssid, "AWifiSSID", 10, MAX_SSID_LEN);
    memccpy(wifi_pass, "passwd", 7, MAX_WIFI_PASS_LEN);
    memccpy(rep_addr, "http://reprap.local", 20, MAX_REP_ADDR_LEN);
    memccpy(rep_pass, "pass", 5, MAX_REP_PASS_LEN);
}

void write_settings_to_nvs() {
    ESP_LOGI(TAG, "Saving settings to NVS");
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open(REPPANEL_NVS, NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, NVS_KEY_WIFI_SSID, wifi_ssid));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, NVS_KEY_WIFI_PASS, wifi_pass));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, NVS_KEY_REPRAP_ADDR, rep_addr));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, NVS_KEY_REPRAP_PASS, rep_pass));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);
    print_settings();
}

void read_settings_nvs() {
    nvs_handle my_handle;
    if (nvs_open(REPPANEL_NVS, NVS_READONLY, &my_handle) == ESP_ERR_NVS_NOT_FOUND) {
        // First start
        nvs_close(my_handle);
        init_settings();
        write_settings_to_nvs();
    } else {
        ESP_LOGI(TAG, "Reading settings from NVS");
        size_t required_size = 0;
        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_WIFI_SSID, NULL, &required_size));
        char *tmp_wifi_ssid = malloc(required_size);
        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_WIFI_SSID, tmp_wifi_ssid, &required_size));
        memccpy(wifi_ssid, tmp_wifi_ssid, required_size, MAX_SSID_LEN);

        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_WIFI_PASS, NULL, &required_size));
        char *tmp_wifi_pass = malloc(required_size);
        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_WIFI_PASS, tmp_wifi_pass, &required_size));
        memccpy(wifi_pass, tmp_wifi_pass, required_size, MAX_WIFI_PASS_LEN);

        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_REPRAP_ADDR, NULL, &required_size));
        char *tmp_rep_addr = malloc(required_size);
        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_REPRAP_ADDR, tmp_rep_addr, &required_size));
        memccpy(rep_addr, tmp_rep_addr, required_size, MAX_REP_ADDR_LEN);

        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_REPRAP_PASS, NULL, &required_size));
        char *tmp_rep_pass = malloc(required_size);
        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_REPRAP_PASS, tmp_rep_pass, &required_size));
        memccpy(rep_pass, tmp_rep_pass, required_size, MAX_REP_PASS_LEN);

        nvs_close(my_handle);
        free(tmp_wifi_pass);
        free(tmp_wifi_ssid);
        free(tmp_rep_addr);
        free(tmp_rep_pass);
        print_settings();
    }
}