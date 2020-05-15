//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <esp_wifi_types.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <string.h>
#include "main.h"
#include <lvgl/src/lv_font/lv_symbol_def.h>
#include <mdns.h>

#include "esp32_settings.h"
#include "reppanel.h"

#define TAG "ESP32WiFi"
#define MAXIMUM_RETRY_WIFI  5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define DEFAULT_SCAN_LIST_SIZE  10

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

int resolve_mdns_host(const char *host_name, char *result_ip) {
    ESP_LOGI(TAG, "Query A: %s", host_name);
    struct ip4_addr addr;
    addr.addr = 0;
    esp_err_t err = mdns_query_a(host_name, 2000, &addr);
    if (err) {
        if (err == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "Host was not found!");
            return 0;
        }
        ESP_LOGI(TAG, "Query failed: %s", esp_err_to_name(err));
        return 0;
    }
    ESP_LOGI(TAG, IPSTR, IP2STR(&addr));
    sprintf(result_ip, "http://"IPSTR, IP2STR(&addr));
    return 1;
}

void mdns_print_results(mdns_result_t *results) {
    const char *if_str[] = {"STA", "AP", "ETH", "MAX"};
    const char *ip_protocol_str[] = {"V4", "V6", "MAX"};
    mdns_result_t *r = results;
    mdns_ip_addr_t *a = NULL;
    int i = 1, t;
    while (r) {
        printf("%d: Interface: %s, Type: %s\n", i++, if_str[r->tcpip_if], ip_protocol_str[r->ip_protocol]);
        if (r->instance_name) {
            printf("  PTR : %s\n", r->instance_name);
        }
        if (r->hostname) {
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if (r->txt_count) {
            printf("  TXT : [%u] ", r->txt_count);
            for (t = 0; t < r->txt_count; t++) {
                printf("%s=%s; ", r->txt[t].key, r->txt[t].value);
            }
            printf("\n");
        }
        a = r->addr;
        while (a) {
            if (a->addr.type == IPADDR_TYPE_V6) {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }
        r = r->next;
    }
}

void find_mdns_service(const char *service_name, const char *proto) {
    ESP_LOGI(TAG, "Query PTR: %s.%s.local", service_name, proto);
    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20, &results);
    if (err) {
        ESP_LOGE(TAG, "Query failed: %s", esp_err_to_name(err));
        return;
    }
    if (!results) {
        ESP_LOGW(TAG, "No results found!");
        return;
    }
    mdns_print_results(results);
    mdns_query_results_free(results);
}

static void start_mdns_service() {
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "MDNS Init failed: %d", err);
        return;
    }
    //set hostname
    mdns_hostname_set("RepPanel");
    //set default instance
    char tmp[20];
    sprintf(tmp, "RepPanel %s", get_version_string());
    mdns_instance_name_set(tmp);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY_WIFI) {
            if (rp_conn_stat != REPPANEL_UART_CONNECTED)
                rp_conn_stat = REPPANEL_WIFI_RECONNECTING;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        if (rp_conn_stat != REPPANEL_UART_CONNECTED)
            rp_conn_stat = REPPANEL_WIFI_DISCONNECTED;
        ESP_LOGI(TAG, "Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got ip:"
                IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        if (rp_conn_stat != REPPANEL_UART_CONNECTED)
            rp_conn_stat = REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED;
    }
    if (xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
        update_rep_panel_conn_status();
        xSemaphoreGive(xGuiSemaphore);
    }
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
    start_mdns_service();
}

/**
 * Call only when GUI semaphore is taken
 */
void reconnect_wifi() {
    rp_conn_stat = REPPANEL_WIFI_RECONNECTING;
    update_rep_panel_conn_status();
    esp_wifi_disconnect();
    ESP_LOGI(TAG, "Reconnecting to %s with password %s", wifi_ssid, wifi_pass);
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, wifi_ssid, MAX_SSID_LEN);
    memcpy(wifi_config.sta.password, wifi_pass, MAX_WIFI_PASS_LEN);
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    esp_wifi_connect();
}

void get_connection_info(char txt_buffer[200]) {
    wifi_ap_record_t ap_info;
    switch (rp_conn_stat) {
        default:
        case REPPANEL_WIFI_DISCONNECTED:
        case REPPANEL_NO_CONNECTION:
            strcpy(txt_buffer, "Not connected to network or UART");
            break;
        case REPPANEL_UART_CONNECTED:
            strcpy(txt_buffer, "Connected via UART");
            break;
        case REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED:
            memset(&ap_info, 0, sizeof(ap_info));
            ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
            sprintf(txt_buffer, "Connected to %s\nSignal: %ddBm\nNo response from printer!", ap_info.ssid,
                    ap_info.rssi);
            break;
        case REPPANEL_WIFI_CONNECTED:
            memset(&ap_info, 0, sizeof(ap_info));
            ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
            sprintf(txt_buffer, "Connected to %s\nSignal: %ddBm", ap_info.ssid, ap_info.rssi);
            break;
        case REPPANEL_WIFI_RECONNECTING:
            strcpy(txt_buffer, "Reconnecting");
    }
}

/**
 * Scan for available WiFi networks. Returns a list of SSIDs separated by a line brake
 * @param aps Filled with SSIDs separated by line brake
 */
void get_avail_wifi_networks(char *aps) {
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    wifi_scan_config_t scan_config;
    memset(&scan_config, 0, sizeof(scan_config));
    scan_config.channel = 0;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    while (err != ESP_OK) {
        ESP_LOGW(TAG, "Error scanning for WiFi: %s", esp_err_to_name(err));
        err = esp_wifi_scan_start(&scan_config, true);
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    if (ap_count > 0) {
        sprintf(aps, "%s", ap_info[0].ssid);
    }
    for (int i = 1; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        char tmp[50];
        sprintf(tmp, "\n%s", ap_info[i].ssid);
        strcat(aps, tmp);
    }
}

/**
 * Scan for available mDNS devices. Format list so that it can be used by a ddlist.
 * @param txt_buffer Elements separated by new line character
 */
void get_avail_duets(char *txt_buffer) {
    txt_buffer[0] = '\0';
    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr("_http", "_tcp", 2000, 20, &results);
    if (err) {
        ESP_LOGE("mDNS Scanner", "Query failed: %s", esp_err_to_name(err));
        return;
    }
    if (!results) {
        ESP_LOGW("mDNS Scanner", "No results found!");
        return;
    } else {
        mdns_result_t *r = results;
        while (r) {
            if (r->hostname) {
                char tmp[32]; tmp[0] = '\0';
                sprintf(tmp, "%s.local\n", &r->hostname[0]);
                strcat(txt_buffer, tmp);
            }
            r = r->next;
        }
        txt_buffer[strlen(txt_buffer) - 1] = '\0';  // remove new line
        ESP_LOGI(TAG, "Found: %s", txt_buffer);
        mdns_query_results_free(results);
    }
}