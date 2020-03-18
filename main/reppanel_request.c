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
#include "reppanel_process.h"

#define TAG                 "RequestTask"
#define JSON_BUFF_SIZE      10240
#define REQUEST_TIMEOUT_MS  150

static char json_buffer[JSON_BUFF_SIZE];
static bool got_filaments = false;
static bool got_status_two = false;
static bool got_duet_settings = false;

const char *_decode_reprap_status(const char *valuestring) {
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

void _process_reprap_status(int type) {
    cJSON *root = cJSON_Parse(json_buffer);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s\n", error_ptr);
        }
        cJSON_Delete(root);
        return;
    }
    cJSON *name = cJSON_GetObjectItem(root, DUET_STATUS);
    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
        strlcpy(reppanel_status, _decode_reprap_status(name->valuestring), MAX_REPRAP_STATUS_LEN);
        if (label_status != NULL) lv_label_set_text(label_status, reppanel_status);
    }

    cJSON *duet_temps = cJSON_GetObjectItem(root, DUET_TEMPS);
    cJSON *duet_temps_bed = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_BED);
    if (duet_temps_bed) {
        if (reprap_bed.temp_hist_curr_pos < (NUM_TEMPS_BUFF - 1)) {
            reprap_bed.temp_hist_curr_pos++;
        } else {
            reprap_bed.temp_hist_curr_pos = 0;
        }
        reprap_bed.temp_buff[reprap_bed.temp_hist_curr_pos] = cJSON_GetObjectItem(duet_temps_bed,
                                                                                  DUET_TEMPS_BED_CURRENT)->valuedouble;
        if (label_bed_temp != NULL)
            lv_label_set_text_fmt(label_bed_temp, "%.1f°%c",
                                  reprap_bed.temp_buff[reprap_bed.temp_hist_curr_pos], get_temp_unit());
    }
    // Get bed heater index
    cJSON *duet_temps_bed_heater = cJSON_GetObjectItem(duet_temps_bed, DUET_TEMPS_BED_HEATER);    // bed heater state
    if (duet_temps_bed_heater && cJSON_IsNumber(duet_temps_bed_heater)) {
        reprap_bed.heater_indx = duet_temps_bed_heater->valueint;
    }
    // Get bed active temp
    cJSON *duet_temps_bed_active = cJSON_GetObjectItem(duet_temps_bed, DUET_TEMPS_ACTIVE);    // bed active temp
    if (duet_temps_bed_active && cJSON_IsNumber(duet_temps_bed_active)) {
        reprap_bed.active_temp = duet_temps_bed_active->valuedouble;
    }
    // Get bed standby temp
    cJSON *duet_temps_bed_standby = cJSON_GetObjectItem(duet_temps_bed, DUET_TEMPS_STANDBY);    // bed active temp
    if (duet_temps_bed_standby && cJSON_IsNumber(duet_temps_bed_standby)) {
        reprap_bed.standby_temp = duet_temps_bed_standby->valuedouble;
    }
    // Get bed heater state
    int _heater_states[MAX_NUM_TOOLS];  // bed heater state must be on pos 0
    cJSON *duet_temps_bed_state = cJSON_GetObjectItem(duet_temps_bed, DUET_TEMPS_BED_STATE);    // bed heater state
    if (duet_temps_bed_state && cJSON_IsNumber(duet_temps_bed_state)) {
        _heater_states[0] = duet_temps_bed_state->valueint;
    }

    // Get tool heater state
    cJSON *duet_temps_state = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_BED_STATE);        // all other heater states
    int pos = 0;
    cJSON *iterator = NULL;
    cJSON_ArrayForEach(iterator, duet_temps_state) {
        if (cJSON_IsNumber(iterator)) {
            if (pos != reprap_bed.heater_indx) {                                            // ignore bed heater
                _heater_states[pos] = iterator->valueint;
            }
            pos++;
        }
    }
    num_heaters = pos;
    num_tools = pos - 1;
    update_heater_status(_heater_states, num_heaters);

    if (type == 2) {
        got_status_two = true;
        // Get tool information
        pos = 0;
        cJSON *tools = cJSON_GetObjectItem(root, DUET_TOOLS);
        if (tools != NULL) {
            cJSON_ArrayForEach(iterator, tools) {
                if (cJSON_IsObject(iterator)) {
                    if (cJSON_IsNumber(cJSON_GetObjectItem(iterator, "number")))
                        reprap_tools[pos].number = cJSON_GetObjectItem(iterator, "number")->valueint;
                    if (cJSON_IsString(cJSON_GetObjectItem(iterator, "name")))
                        strncpy(reprap_tools[pos].name, cJSON_GetObjectItem(iterator, "name")->valuestring,
                                MAX_TOOL_NAME_LEN);
                    if (cJSON_IsString(cJSON_GetObjectItem(iterator, "filament")))
                        strncpy(reprap_tools[pos].filament, cJSON_GetObjectItem(iterator, "filament")->valuestring,
                                MAX_FILA_NAME_LEN);
                    if (cJSON_IsArray(cJSON_GetObjectItem(iterator, "heaters"))) {
                        // Ignore multiple heaters per tool
                        cJSON *heaterindx_item = cJSON_GetArrayItem(cJSON_GetObjectItem(iterator, "heaters"), 0);
                        reprap_tools[pos].heater_indx = heaterindx_item->valueint;
                    }
                    pos++;
                }
            }
            if (label_extruder_name != NULL)
                lv_label_set_text(label_extruder_name, reprap_tools[current_visible_tool_indx].name);
        }
        num_tools = pos;

        // Get firmware information
        cJSON *mcutemp = cJSON_GetObjectItem(root, DUET_MCU_TEMP);
        reprap_mcu_temp = cJSON_GetObjectItem(mcutemp, "cur")->valuedouble;
        cJSON *firmware_name = cJSON_GetObjectItem(root, DUET_FIRM_NAME);
        strncpy(reprap_firmware_name, firmware_name->valuestring, sizeof(reprap_firmware_name));
        cJSON *firmware_version = cJSON_GetObjectItem(root, DUET_FIRM_VER);
        strncpy(reprap_firmware_version, firmware_version->valuestring, sizeof(reprap_firmware_version));
    }

    // Get current tool temperatures
    cJSON *duet_temps_current = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_CURRENT);
    if (duet_temps_current) {
        for (int i = 0; i < num_tools; i++) {
            if (reprap_tools[i].temp_hist_curr_pos < (NUM_TEMPS_BUFF - 1)) {
                reprap_tools[i].temp_hist_curr_pos++;
            } else {
                reprap_tools[i].temp_hist_curr_pos = 0;
            }
            reprap_tools[i].temp_buff[reprap_tools[i].temp_hist_curr_pos] = cJSON_GetArrayItem(duet_temps_current,
                                                                                               reprap_tools[i].heater_indx)->valuedouble;
        }
        if (label_tool_temp != NULL)
            lv_label_set_text_fmt(label_tool_temp, "%.1f°%c",
                                  reprap_tools[current_visible_tool_indx].temp_buff[reprap_tools[current_visible_tool_indx].temp_hist_curr_pos],
                                  get_temp_unit());
        if (label_chamber_temp != NULL)
            lv_label_set_text_fmt(label_chamber_temp, "%.1f°%c",
                                  reprap_tools[current_visible_tool_indx].temp_buff[reprap_tools[current_visible_tool_indx].temp_hist_curr_pos],
                                  get_temp_unit());
    }

    cJSON *print_progess = cJSON_GetObjectItem(root, REPRAP_FRAC_PRINTED);
    if (cJSON_IsNumber(print_progess)) {
        sprintf(reppanel_job_progess, "%.0f%%", print_progess->valuedouble);
        //lv_label_set_text(label_progress_percent, reppanel_job_progess);
    }
    cJSON_Delete(root);
}

void _process_duet_settings() {
    ESP_LOGI(TAG, "Processing D2WC status json");
    got_duet_settings = true;
    cJSON *root = cJSON_Parse(json_buffer);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s\n", error_ptr);
        }
        cJSON_Delete(root);
        return;
    }
    cJSON *machine = cJSON_GetObjectItem(root, "machine");
    reprap_babysteps_amount = cJSON_GetObjectItem(machine, "babystepAmount")->valuedouble;
    reprap_move_feedrate = cJSON_GetObjectItem(machine, "moveFeedrate")->valuedouble;
    cJSON *machine_extruder_amounts = cJSON_GetObjectItem(machine, "extruderAmounts");
    cJSON *machine_extruder_feedrates = cJSON_GetObjectItem(machine, "extruderFeedrates");

    cJSON *machine_temps = cJSON_GetObjectItem(machine, "temperatures");
    cJSON *machine_temps_tool = cJSON_GetObjectItem(machine_temps, "tool");
    cJSON *machine_temps_tool_active = cJSON_GetObjectItem(machine_temps_tool, "active");
    cJSON *machine_temps_tool_standby = cJSON_GetObjectItem(machine_temps_tool, "standby");

    cJSON *machine_temps_bed = cJSON_GetObjectItem(machine_temps, "bed");
    cJSON *machine_temps_bed_active = cJSON_GetObjectItem(machine_temps_bed, "active");
    cJSON *machine_temps_bed_standby = cJSON_GetObjectItem(machine_temps_bed, "standby");

    cJSON *iterator = NULL;
    int pos = 0;
    cJSON_ArrayForEach(iterator, machine_extruder_amounts) {
        if (cJSON_IsNumber(iterator)) {
            reprap_extruder_amounts[pos] = iterator->valuedouble;
            pos++;
        }
    }
    pos = 0;
    cJSON_ArrayForEach(iterator, machine_extruder_feedrates) {
        if (cJSON_IsNumber(iterator)) {
            reprap_extruder_feedrates[pos] = iterator->valuedouble;
            pos++;
        }
    }

    pos = 0;
    cJSON_ArrayForEach(iterator, machine_temps_tool_active) {
        if (cJSON_IsNumber(iterator)) {
            reprap_tool_poss_temps.temps_active[pos] = iterator->valuedouble;
            pos++;
        }
    }

    pos = 0;
    cJSON_ArrayForEach(iterator, machine_temps_tool_standby) {
        if (cJSON_IsNumber(iterator)) {
            reprap_tool_poss_temps.temps_standby[pos] = iterator->valuedouble;
            pos++;
        }
    }

    pos = 0;
    cJSON_ArrayForEach(iterator, machine_temps_bed_standby) {
        if (cJSON_IsNumber(iterator)) {
            reprap_bed_poss_temps.temps_standby[pos] = iterator->valuedouble;
            pos++;
        }
    }

    pos = 0;
    cJSON_ArrayForEach(iterator, machine_temps_bed_active) {
        if (cJSON_IsNumber(iterator)) {
            reprap_bed_poss_temps.temps_active[pos] = iterator->valuedouble;
            pos++;
        }
    }

    cJSON_Delete(root);
}

void _process_reprap_filelist() {
    cJSON *root = cJSON_Parse(json_buffer);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s\n", error_ptr);
        }
        return;
    }
    cJSON *err_resp = cJSON_GetObjectItem(root, "err");
    if (err_resp) {
        ESP_LOGE(TAG, "reprap_filelist - Duet responded with error code %i", err_resp->valueint);
        ESP_LOGE(TAG, "%s", cJSON_Print(root));
        cJSON_Delete(root);
        return;
    }
    cJSON *dir_name = cJSON_GetObjectItem(root, "dir");
    if (strncmp("0:/filaments", dir_name->valuestring, 12) == 0) {
        ESP_LOGI(TAG, "Processing filament names");
        got_filaments = true;

        cJSON *filament_folders = cJSON_GetObjectItem(root, "files");
        cJSON *iterator = NULL;
        int pos = 0;
        filament_names[0] = '\0';
        cJSON_ArrayForEach(iterator, filament_folders) {
            if (cJSON_IsObject(iterator)) {
                if (strncmp("d", cJSON_GetObjectItem(iterator, "type")->valuestring, 1) == 0) {
                    if (pos != 0) strncat(filament_names, "\n", MAX_LEN_STR_FILAMENT_LIST - strlen(filament_names));
                    strncat(filament_names, cJSON_GetObjectItem(iterator, "name")->valuestring,
                            MAX_LEN_STR_FILAMENT_LIST - strlen(filament_names));
                    pos++;
                }
            }
        }
        ESP_LOGI(TAG, "Filament names\n%s", filament_names);
    } else if (strncmp("0:/macros", dir_name->valuestring, 9) == 0) {
        ESP_LOGI(TAG, "Processing macros");
        cJSON *_folders = cJSON_GetObjectItem(root, "files");
        cJSON *iterator = NULL;
        int pos = 0;
        for (int i = 0; i < MAX_NUM_MACROS; i++) {
            free(reprap_macro_names[pos]);
        }
        cJSON_ArrayForEach(iterator, _folders) {
            if (cJSON_IsObject(iterator)) {
                if (strncmp("f", cJSON_GetObjectItem(iterator, "type")->valuestring, 1) == 0 && pos < MAX_NUM_MACROS) {
                    reprap_macro_names[pos] = malloc(strlen(cJSON_GetObjectItem(iterator, "name")->valuestring) + 1);
                    strcpy(reprap_macro_names[pos], cJSON_GetObjectItem(iterator, "name")->valuestring);
                    pos++;
                }
            }
        }
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
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            // ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            // printf("%.*s", evt->data_len, (char *) evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
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
            // ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
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
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
//        ESP_LOGI(TAG, "Status = %d, content_length = %d",
//                 esp_http_client_get_status_code(client),
//                 esp_http_client_get_content_length(client));
    }
    esp_http_client_cleanup(client);
    // if (get_d2wc_config) reprap_wifi_download("0%3A%2Fsys%2Fdwc2settings.json");
}

void wifi_duet_get_status(int type) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_status?type=%i", rep_addr, type);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
//        ESP_LOGI(TAG, "Status = %d, content_length = %d",
//                 esp_http_client_get_status_code(client),
//                 esp_http_client_get_content_length(client));

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                _process_reprap_status(type);
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
    char encoded_gcode[strlen(gcode) * 3];
    url_encode((unsigned char *) gcode, encoded_gcode);
    sprintf(request_addr, "%s/rr_gcode?gcode=%s", rep_addr, encoded_gcode);
    ESP_LOGV(TAG, "%s", request_addr);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
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
    char encoded_dir[strlen(directory) * 3];
    url_encode((unsigned char *) directory, encoded_dir);
    sprintf(request_addr, "%s/rr_filelist?dir=%s", rep_addr, encoded_dir);
    ESP_LOGI(TAG, "%s", request_addr);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
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
            _process_reprap_filelist();
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
    char encoded_filename[strlen(filename) * 3];
    url_encode((unsigned char *) filename, encoded_filename);
    sprintf(request_addr, "%s/rr_fileinfo?dir=%s", rep_addr, encoded_filename);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
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
            .timeout_ms = REQUEST_TIMEOUT_MS,
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
            .timeout_ms = REQUEST_TIMEOUT_MS,
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
                _process_duet_settings();
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

void reprap_send_gcode(char *gcode_command) {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        reprap_wifi_send_gcode(gcode_command);
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Writing to UART not supported for now");
    }
}

void request_filaments() {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting filaments");
        reprap_wifi_get_filelist("0:/filaments&first=0");
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Using UART not supported for now");
    }
}

void request_macros() {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting macros");
        reprap_wifi_get_filelist("0:/macros&first=0");
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Using UART not supported for now");
    }
}

/**
 * Called most of the time
 * @param task
 */
void request_reprap_status_updates(lv_task_t *task) {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        if (!got_duet_settings) reprap_wifi_download("0%3A%2Fsys%2Fdwc2settings.json");
        if (!got_filaments) request_filaments();
        if (!got_status_two) wifi_duet_get_status(2);
        wifi_duet_get_status(1);
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Using UART not supported for now");
    }
}

/**
 * Get additional status info from printer. No not call too often.
 * @param task
 */
void request_reprap_ext_status_updates(lv_task_t *task) {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        if (!got_duet_settings) reprap_wifi_download("0%3A%2Fsys%2Fdwc2settings.json");
        wifi_duet_get_status(2);
    }
}