//
// Created by cyber on 24.02.20.
// https://github.com/chrishamm/DuetWebControl/tree/legacy

#include <lvgl/src/lv_misc/lv_task.h>
#include <esp_log.h>
#include <lwip/ip4_addr.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <mdns.h>
#include <lvgl/lvgl.h>
#include "duet_status_json.h"
#include "reppanel.h"
#include "reppanel_request.h"

#define TAG                 "RequestTask"
#define JSON_BUFF_SIZE      1024

static char json_buffer[JSON_BUFF_SIZE];

const char *decode_reprap_status(const char *valuestring) {
    switch (*valuestring) {
        case REPRAP_STATUS_PROCESS_CONFIG:
            return "Reading config";
        case REPRAP_STATUS_IDLE:
            return "Idle";
        case REPRAP_STATUS_BUSY:
            return "Busy";
        case REPRAP_STATUS_PRINTING:
            return "Printing";
        case REPRAP_STATUS_DECELERATING:
            return "Decelerating";
        case REPRAP_STATUS_STOPPED:
            return "Stopped";
        case REPRAP_STATUS_RESUMING:
            return "Resuming";
        case REPRAP_STATUS_HALTED:
            return "Halted";
        case REPRAP_STATUS_FLASHING:
            return "Flashing";
        case REPRAP_STATUS_CHANGINGTOOL:
            return "Tool change";
        default:
            break;
    }
    return "Unknown status";
}

void process_reprap_status(int type) {
    cJSON *root = cJSON_Parse(json_buffer);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s\n", error_ptr);
        }
        return;
    }
    static char one_decimal_txt[6];
    cJSON *name = cJSON_GetObjectItem(root, DUET_STATUS);
    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
        lv_label_set_text(label_status, decode_reprap_status(name->valuestring));
    }

    cJSON *duet_temps = cJSON_GetObjectItem(root, DUET_TEMPS);
    cJSON *duet_temps_bed = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_BED);
    if (duet_temps_bed) {
        sprintf(one_decimal_txt, "%.1f", cJSON_GetObjectItem(duet_temps_bed, DUET_TEMPS_BED_CURRENT)->valuedouble);
        lv_label_set_text(label_bed_temp, one_decimal_txt);
    }

    cJSON *duet_temps_current = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_CURRENT);
    if (duet_temps_current) {
        sprintf(one_decimal_txt, "%.1f", cJSON_GetArrayItem(duet_temps_current, 1)->valuedouble);
        //lv_label_set_text(label_tool1_temp, one_decimal_txt);
    }

    cJSON *print_progess = cJSON_GetObjectItem(root, REPRAP_FRAC_PRINTED);
    if (cJSON_IsNumber(print_progess)) {
        sprintf(one_decimal_txt, "%.0f%%", print_progess->valuedouble);
        //lv_label_set_text(label_progress_percent, one_decimal_txt);
    }

    cJSON_Delete(root);
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    static int json_buffer_pos = 0;
    if (esp_http_client_get_status_code(evt->client) == 401) {
        ESP_LOGW(TAG, "Need to authorise first. Ignoring data.");
        return ESP_OK;
    }
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            // ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            // printf("%.*s", evt->data_len, (char *) evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if ((json_buffer_pos + evt->data_len) < JSON_BUFF_SIZE) {
                    strncpy(&json_buffer[json_buffer_pos], (char *) evt->data, evt->data_len);
                    json_buffer_pos += evt->data_len;
                } else {
                    ESP_LOGE(TAG, "Status-JSON buffer overflow (%i >= %i). Resetting!",
                             (evt->data_len + json_buffer_pos), JSON_BUFF_SIZE);
                    json_buffer_pos = 0;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            json_buffer_pos = 0;
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

void wifi_duet_get_status(int type) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "http://%s.local/rr_status?type=%i", PRINTER_NAME, type);
    esp_http_client_config_t config = {
            .url = request_addr,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    switch (esp_http_client_get_status_code(client)) {
        case 200:
            process_reprap_status(type);
            break;
        case 401:
            ESP_LOGI(TAG, "Authorising with Duet");
            wifi_duet_authorise();
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void wifi_send_gcode(char *gcode) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "http://%s.local/rr_gcode?gcode=%s", PRINTER_NAME, gcode);
    esp_http_client_config_t config = {
            .url = request_addr,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    switch (esp_http_client_get_status_code(client)) {
        case 200:
            // TODO process_reprap_gcode();
            break;
        case 401:
            ESP_LOGI(TAG, "Authorising with Duet");
            wifi_duet_authorise();
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void wifi_get_filelist(char *directory) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "http://%s.local/rr_filelist?dir=%s", PRINTER_NAME, directory);
    esp_http_client_config_t config = {
            .url = request_addr,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    switch (esp_http_client_get_status_code(client)) {
        case 200:
            // TODO process_reprap_filelist();
            break;
        case 401:
            ESP_LOGI(TAG, "Authorising with Duet");
            wifi_duet_authorise();
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void wifi_get_fileinfo(char *filename) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "http://%s.local/rr_fileinfo?dir=%s", PRINTER_NAME, filename);
    esp_http_client_config_t config = {
            .url = request_addr,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    switch (esp_http_client_get_status_code(client)) {
        case 200:
            // TODO process_reprap_fileinfo();
            break;
        case 401:
            ESP_LOGI(TAG, "Authorising with Duet");
            wifi_duet_authorise();
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void wifi_get_config() {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "http://%s.local/rr_config", PRINTER_NAME);
    esp_http_client_config_t config = {
            .url = request_addr,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    switch (esp_http_client_get_status_code(client)) {
        case 200:
            // TODO process_reprap_config();
            break;
        case 401:
            ESP_LOGI(TAG, "Authorising with Duet");
            wifi_duet_authorise();
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void request_reprap_status_updates(lv_task_t *task) {
    wifi_duet_get_status(1);
    //wifi_duet_get_status(3);
    sleep(1);
}