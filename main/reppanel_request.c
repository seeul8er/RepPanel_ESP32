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
    cJSON *name = cJSON_GetObjectItem(root, DUET_STATUS);
    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
        strlcpy(reppanel_status, decode_reprap_status(name->valuestring), MAX_REPRAP_STATUS_LEN);
        if (label_status != NULL) lv_label_set_text(label_status, reppanel_status);
    }

    cJSON *duet_temps = cJSON_GetObjectItem(root, DUET_TEMPS);
    cJSON *duet_temps_bed = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_BED);
    if (duet_temps_bed) {
        sprintf(reppanel_bed_temp, "%.1f°%c", cJSON_GetObjectItem(duet_temps_bed, DUET_TEMPS_BED_CURRENT)->valuedouble,
                get_temp_unit());
        if (label_bed_temp != NULL) lv_label_set_text(label_bed_temp, reppanel_bed_temp);
    }

    cJSON *duet_temps_current = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_CURRENT);
    if (duet_temps_current) {
        sprintf(reppanel_tool_temp, "%.1f°%c", cJSON_GetArrayItem(duet_temps_current, 1)->valuedouble, get_temp_unit());
        if (label_tool_temp != NULL) lv_label_set_text(label_tool_temp, reppanel_tool_temp);
        if (label_chamber_temp != NULL) lv_label_set_text(label_chamber_temp, reppanel_tool_temp);
    }

    cJSON *print_progess = cJSON_GetObjectItem(root, REPRAP_FRAC_PRINTED);
    if (cJSON_IsNumber(print_progess)) {
        sprintf(reppanel_job_progess, "%.0f%%", print_progess->valuedouble);
        //lv_label_set_text(label_progress_percent, reppanel_job_progess);
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

void wifi_duet_authorise(bool get_d2wc_config) {
    char printer_url[MAX_REQ_ADDR_LENGTH];
    sprintf(printer_url, "%s/rr_connect?password=%s", rep_addr, rep_pass);
    esp_http_client_config_t config = {
            .url = printer_url,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    esp_http_client_cleanup(client);
    if (get_d2wc_config) reprap_wifi_download("0%3A%2Fsys%2Fdwc2settings.json");
}

void wifi_duet_get_status(int type) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_status?type=%i", rep_addr, type);
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

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                process_reprap_status(type);
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(true);
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error requesting RepRap status: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void reprap_wifi_send_gcode(char *gcode) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_gcode?gcode=%s", rep_addr, gcode);
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
            wifi_duet_authorise(false);
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void reprap_wifi_get_filelist(char *directory) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_filelist?dir=%s", rep_addr, directory);
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
            wifi_duet_authorise(false);
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void reprap_wifi_get_fileinfo(char *filename) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_fileinfo?dir=%s", rep_addr, filename);
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
            wifi_duet_authorise(false);
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void reprap_wifi_get_config() {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_config", rep_addr);
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
            wifi_duet_authorise(false);
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void reprap_wifi_download(char *file) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_download?name=%s", rep_addr, file);
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

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                // TODO: process Duet 2 Web Control settings
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(false);
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error requesting RepRap status: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void request_reprap_status_updates(lv_task_t *task) {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        wifi_duet_get_status(1);
        //wifi_duet_get_status(3);
    }
}