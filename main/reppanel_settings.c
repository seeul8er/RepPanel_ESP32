//
// Created by cyber on 08.03.20.
//
#include <nvs.h>
#include <string.h>
#include <esp_log.h>
#include "reppanel_settings.h"

#define TAG "RepPanelSettings"

void print_settings() {
    ESP_LOGI(TAG, "Wifi SSID: %s", wifi_ssid);
    ESP_LOGI(TAG, "Wifi password: %s", wifi_pass);
    ESP_LOGI(TAG, "RepRap addr: %s", rep_addr);
    ESP_LOGI(TAG, "RepRap password: %s", rep_pass);
}

void init_settings() {
    memccpy(wifi_ssid, "AWifiSSID", 10, 32);
    memccpy(wifi_pass, "AWifipasswd", 12, 64);
    memccpy(rep_addr, "http://reprap.local", 20, 200);
    memccpy(rep_pass, "RepRapPass", 11, 64);
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
        memccpy(wifi_ssid, tmp_wifi_ssid, required_size, 32);

        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_WIFI_PASS, NULL, &required_size));
        char *tmp_wifi_pass = malloc(required_size);
        ESP_ERROR_CHECK(nvs_get_str(my_handle, NVS_KEY_WIFI_PASS, tmp_wifi_pass, &required_size));
        memccpy(wifi_pass, tmp_wifi_pass, required_size, 64);

        nvs_close(my_handle);
        free(tmp_wifi_pass);
        free(tmp_wifi_ssid);
        print_settings();
    }
}