//
// Created by cyber on 24.02.20.
// https://github.com/chrishamm/DuetWebControl/tree/legacy

#include <lvgl/src/lv_misc/lv_task.h>
#include <esp_log.h>
#include <lwip/ip4_addr.h>
#include <esp_http_client.h>
#include "../../../esp32_SDK/esp/esp-idf/components/mdns/include/mdns.h"

#define TAG "RequestTask"

void resolve_mdns_host(const char *host_name) {
    ESP_LOGV(TAG, "Query A: %s.local", host_name);

    struct ip4_addr addr;
    addr.addr = 0;

    esp_err_t err = mdns_query_a(host_name, 5000, &addr);
    if (err) {
        if (err == ESP_ERR_NOT_FOUND) {
            ESP_LOGV(TAG, "Host was not found!");
            return;
        }
        ESP_LOGW(TAG, "Query Failed");
        return;
    }
    ESP_LOGV(TAG, IPSTR, IP2STR(&addr));
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char *) evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                printf("%.*s", evt->data_len, (char *) evt->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void wifi_duet_authorise() {
    esp_http_client_config_t config = {
            .url = "http://CyberPrint.local/rr_connect?password=deimam",
            .event_handler = _http_event_handle,
    }; // http://cyberprint.local/rr_connect?password=deimam&time=2020-2-24T22%3A59%3A0
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    esp_http_client_cleanup(client);
}

void wifi_duet_get_status() {
    esp_http_client_config_t config = {
            .url = "http://CyberPrint.local/rr_status?type=1",
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    if (esp_http_client_get_status_code(client) == 401) {
        ESP_LOGI(TAG, "Authorising first");
        wifi_duet_authorise();
    }
    esp_http_client_cleanup(client);
}

void request_reprap_status(lv_task_t *task) {
    uint32_t *user_data = task->user_data;
    wifi_duet_get_status();
}