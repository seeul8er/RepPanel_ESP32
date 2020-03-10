//
// Created by cyber on 10.03.20.
//

#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <string.h>
#include <lvgl/src/lv_font/lv_symbol_def.h>

#include "esp32_settings.h"
#include "reppanel.h"

#define TAG "ESP32WiFi"
#define MAXIMUM_RETRY_WIFI  5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY_WIFI) {
            reppanel_conn_status = 3;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        reppanel_conn_status = 2;
        ESP_LOGI(TAG, "Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got ip:"
        IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        reppanel_conn_status = 1;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    update_rep_panel_conn_status();
}

void wifi_init_sta() {
    s_wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, wifi_ssid, MAX_SSID_LEN);
    memcpy(wifi_config.sta.password, wifi_pass, MAX_WIFI_PASS_LEN);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void reconnect_wifi() {
    ESP_LOGI(TAG, "Reconnecting to %s with password %s", wifi_ssid, wifi_pass);
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, wifi_ssid, MAX_SSID_LEN);
    memcpy(wifi_config.sta.password, wifi_pass, MAX_WIFI_PASS_LEN);
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    esp_wifi_connect();
}