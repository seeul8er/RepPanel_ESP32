//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <lvgl/src/lv_misc/lv_task.h>
#include <esp_log.h>
#include <lwip/ip4_addr.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <lvgl/lvgl.h>
#include "duet_status_json.h"
#include "reppanel.h"
#include "reppanel_request.h"
#include "reppanel_process.h"
#include "reppanel_jobstatus.h"
#include "main.h"
#include "reppanel_macros.h"
#include "reppanel_jobselect.h"
#include "reppanel_console.h"
#include "reppanel_machine.h"

#define TAG                 "RequestTask"
#define REQUEST_TIMEOUT_MS  150

static response_buff_t resp_buff_status_update_task;
static response_buff_t resp_buff_filelist_task;
static response_buff_t resp_buff_gui_task;

static bool got_filaments = false;
static bool got_status_two = false;
static bool got_duet_settings = false;
static int status_request_err_cnt = 0;      // request errors in a row
bool job_paused = false;
int seq_num_msgbox = -1;

const char *_decode_reprap_status(const char *valuestring) {
    job_paused = false;
    reprap_status = *valuestring;
    switch (*valuestring) {
        case REPRAP_STATUS_PROCESS_CONFIG:
            job_running = false;
            return "Reading config";
        case REPRAP_STATUS_IDLE:
            job_running = false;
            return "Idle";
        case REPRAP_STATUS_BUSY:
            job_running = false;
            return "Busy";
        case REPRAP_STATUS_PRINTING:
            job_running = true;
            return "Printing";
        case REPRAP_STATUS_DECELERATING:
            job_running = false;
            return "Decelerating";
        case REPRAP_STATUS_STOPPED:
            job_running = true;
            job_paused = true;
            return "Paused";
        case REPRAP_STATUS_RESUMING:
            job_running = true;
            return "Resuming";
        case REPRAP_STATUS_HALTED:
            job_running = false;
            return "Halted";
        case REPRAP_STATUS_FLASHING:
            job_running = false;
            return "Flashing";
        case REPRAP_STATUS_CHANGINGTOOL:
            job_running = true;
            return "Tool change";
        case REPRAP_STATUS_SIMULATING:
            job_running = true;
            return "Simulating";
        default:
            break;
    }
    return "UnknownStatus";
}

void _process_reprap_status(char *buff, int type) {
    cJSON *root = cJSON_Parse(buff);
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
    }

    cJSON *coords = cJSON_GetObjectItem(root, "coords");
    if (coords) {
        cJSON *axesHomed = cJSON_GetObjectItem(coords, "axesHomed");
        if (axesHomed && cJSON_IsArray(axesHomed)) {
            reprap_axes.x_homed = cJSON_GetArrayItem(axesHomed, 0)->valueint == 1;
            reprap_axes.y_homed = cJSON_GetArrayItem(axesHomed, 1)->valueint == 1;
            reprap_axes.z_homed = cJSON_GetArrayItem(axesHomed, 2)->valueint == 1;
        }
        cJSON *xyz = cJSON_GetObjectItem(coords, "xyz");
        if (xyz && cJSON_IsArray(xyz)) {
            reprap_axes.x = cJSON_GetArrayItem(xyz, 0)->valuedouble;
            reprap_axes.y = cJSON_GetArrayItem(xyz, 1)->valuedouble;
            reprap_axes.z = cJSON_GetArrayItem(xyz, 2)->valuedouble;
        }
    }

    int _heater_states[MAX_NUM_TOOLS];  // bed heater state must be on pos 0
    cJSON *duet_temps = cJSON_GetObjectItem(root, DUET_TEMPS);
    if (duet_temps) {
        cJSON *duet_temps_bed = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_BED);
        if (duet_temps_bed) {
            if (reprap_bed.temp_hist_curr_pos < (NUM_TEMPS_BUFF - 1)) {
                reprap_bed.temp_hist_curr_pos++;
            } else {
                reprap_bed.temp_hist_curr_pos = 0;
            }
            reprap_bed.temp_buff[reprap_bed.temp_hist_curr_pos] = cJSON_GetObjectItem(duet_temps_bed,
                                                                                      DUET_TEMPS_BED_CURRENT)->valuedouble;
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
        cJSON *duet_temps_bed_state = cJSON_GetObjectItem(duet_temps_bed, DUET_TEMPS_BED_STATE);    // bed heater state
        if (duet_temps_bed_state && cJSON_IsNumber(duet_temps_bed_state)) {
            _heater_states[0] = duet_temps_bed_state->valueint;
        }
    }

    bool disp_msg = false;
    bool disp_msgbox = false;
    char *msg_txt = "";
    cJSON *duet_output = cJSON_GetObjectItem(root, "output");
    if (duet_output) {
        cJSON *duet_output_msg = cJSON_GetObjectItem(duet_output, "message");
        if (duet_output_msg && cJSON_IsString(duet_output_msg)) {
            disp_msg = true;
            msg_txt = duet_output_msg->valuestring;
        }
        // Right now we only have a msg box for manual bed calibration
        cJSON *duet_output_msgbox = cJSON_GetObjectItem(duet_output, "msgBox");
        if (duet_output_msgbox) {
            cJSON *seq = cJSON_GetObjectItem(duet_output_msgbox, "seq");
            // Beware. This is dirty. Check if we want to show this msg box. We might already display it
            if (seq->valueint != seq_num_msgbox) {
                seq_num_msgbox = seq->valueint;
                disp_msgbox = true;
            }
        }
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
        }
        num_tools = pos;

        // Get firmware information
        cJSON *mcutemp = cJSON_GetObjectItem(root, DUET_MCU_TEMP);
        if (mcutemp)
            reprap_mcu_temp = cJSON_GetObjectItem(mcutemp, "cur")->valuedouble;
        cJSON *firmware_name = cJSON_GetObjectItem(root, DUET_FIRM_NAME);
        if (firmware_name)
            strncpy(reprap_firmware_name, firmware_name->valuestring, sizeof(reprap_firmware_name));
        cJSON *firmware_version = cJSON_GetObjectItem(root, DUET_FIRM_VER);
        if (firmware_version)
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
    }
    // Get active & standby tool temperatures. As for now there is only support one heater per tool
    cJSON *duet_temps_tools = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_TOOLS);
    cJSON *duet_temps_tools_active = cJSON_GetObjectItem(duet_temps_tools, DUET_TEMPS_ACTIVE);
    cJSON *duet_temps_tools_standby = cJSON_GetObjectItem(duet_temps_tools, DUET_TEMPS_STANDBY);
    if (duet_temps_tools_active && cJSON_IsArray(duet_temps_tools_active) && duet_temps_tools_standby &&
        cJSON_IsArray(duet_temps_tools_standby)) {
        for (int i = 0; i < num_tools; i++) {
            cJSON *tool_active_temps_arr = cJSON_GetArrayItem(duet_temps_tools_active,
                                                              reprap_tools[i].number);
            cJSON *tool_standby_temps_arr = cJSON_GetArrayItem(duet_temps_tools_standby,
                                                               reprap_tools[i].number);
            reprap_tools[i].active_temp = cJSON_GetArrayItem(tool_active_temps_arr,
                                                             0)->valuedouble;
            reprap_tools[i].standby_temp = cJSON_GetArrayItem(tool_standby_temps_arr,
                                                              0)->valuedouble;
        }
    }

    if (type == 3) {
        // print job status
        cJSON *print_progess = cJSON_GetObjectItem(root, REPRAP_FRAC_PRINTED);
        if (cJSON_IsNumber(print_progess)) {
            reprap_job_percent = print_progess->valuedouble;
        }

        cJSON *job_dur = cJSON_GetObjectItem(root, REPRAP_JOB_DUR);
        if (job_dur && cJSON_IsNumber(job_dur)) {
            reprap_job_duration = job_dur->valuedouble;
        }

        cJSON *job_curr_layer = cJSON_GetObjectItem(root, REPRAP_CURR_LAYER);
        if (job_curr_layer && cJSON_IsNumber(job_curr_layer)) {
            reprap_job_curr_layer = job_curr_layer->valueint;
        }
    }

    if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 100) == pdTRUE) {
        if (label_status != NULL) lv_label_set_text(label_status, reppanel_status);
        update_ui_machine();
        update_bed_temps_ui();  // update UI with new values
        update_heater_status_ui(_heater_states, num_heaters);  // update UI with new values
        update_current_tool_temps_ui();     // update UI with new values
        if (type == 2 && label_extruder_name != NULL) {
            lv_label_set_text(label_extruder_name, reprap_tools[current_visible_tool_indx].name);
        }
        if (type == 3) update_print_job_status_ui();
        if (disp_msg) reppanel_disp_msg(msg_txt);
        if (disp_msgbox) show_height_adjust_dialog();
        update_rep_panel_conn_status();
        xSemaphoreGive(xGuiSemaphore);
    }

    cJSON_Delete(root);
}

void _process_duet_settings(char *buff) {
    ESP_LOGI(TAG, "Processing D2WC status json");
    got_duet_settings = true;
    cJSON *root = cJSON_Parse(buff);
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

void _process_reprap_filelist(char *buffer) {
    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s\n", error_ptr);
        }
        cJSON_Delete(root);
        return;
    }
    cJSON *err_resp = cJSON_GetObjectItem(root, "err");
    if (err_resp) {
        ESP_LOGE(TAG, "reprap_filelist - Duet responded with error code %i", err_resp->valueint);
        ESP_LOGE(TAG, "%s", cJSON_Print(root));
        cJSON_Delete(root);
        return;
    }
    cJSON *next = cJSON_GetObjectItem(root, "next");
    if (next != NULL && next->valueint != 0) {
        // TODO: Not only get first list. Get all items. Check for next item
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
        for (int i = 0; i < MAX_NUM_MACROS; i++) {
            if (reprap_macros[i].element != NULL) {
                free(((reprap_macro_t *) reprap_macros[i].element)->name);
                free(((reprap_macro_t *) reprap_macros[i].element)->last_mod);
                free(((reprap_macro_t *) reprap_macros[i].element)->dir);
                free((reprap_macro_t *) reprap_macros[i].element);
                reprap_macros[i].element = NULL;
            }
            reprap_macros[i].type = TREE_EMPTY_ELEM;
        }
        int pos = 0;
        cJSON_ArrayForEach(iterator, _folders) {
            if (cJSON_IsObject(iterator)) {
                if (pos < MAX_NUM_MACROS) {
                    if (reprap_macros[pos].element == NULL) {
                        reprap_macros[pos].element = (reprap_macro_t *) malloc(sizeof(reprap_macro_t));
                    }
                    ((reprap_macro_t *) reprap_macros[pos].element)->name = malloc(
                            strlen(cJSON_GetObjectItem(iterator, "name")->valuestring) + 1);
                    ((reprap_macro_t *) reprap_macros[pos].element)->dir = malloc(strlen(dir_name->valuestring) + 1);
                    ((reprap_macro_t *) reprap_macros[pos].element)->last_mod = malloc(1 + 1);
                    strcpy(((reprap_macro_t *) reprap_macros[pos].element)->name,
                           cJSON_GetObjectItem(iterator, "name")->valuestring);
                    strcpy(((reprap_macro_t *) reprap_macros[pos].element)->dir, dir_name->valuestring);
                    if (strncmp("f", cJSON_GetObjectItem(iterator, "type")->valuestring, 1) == 0) {
                        reprap_macros[pos].type = TREE_FILE_ELEM;
                    } else {
                        reprap_macros[pos].type = TREE_FOLDER_ELEM;
                    }
                    pos++;
                }
            }
        }
        if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 100) == pdTRUE) {
            update_macro_list_ui();
            xSemaphoreGive(xGuiSemaphore);
        }
    } else if (strncmp("0:/gcodes", dir_name->valuestring, 9) == 0) {
        ESP_LOGI(TAG, "Processing jobs");
        cJSON *_folders = cJSON_GetObjectItem(root, "files");
        cJSON *iterator = NULL;
        for (int i = 0; i < MAX_NUM_JOBS; i++) {  // clear array
            if (reprap_jobs[i].element != NULL) {
                free(((reprap_job_t *) reprap_jobs[i].element)->name);
                free(((reprap_job_t *) reprap_jobs[i].element)->last_mod);
                free(((reprap_job_t *) reprap_jobs[i].element)->generator);
                free(((reprap_job_t *) reprap_jobs[i].element)->dir);
                free((reprap_job_t *) reprap_jobs[i].element);
                reprap_jobs[i].element = NULL;
            }
            reprap_jobs[i].type = TREE_EMPTY_ELEM;
        }
        int pos = 0;
        cJSON_ArrayForEach(iterator, _folders) {
            if (cJSON_IsObject(iterator)) {
                if (pos < MAX_NUM_JOBS) {
                    if (reprap_jobs[pos].element == NULL) {
                        reprap_jobs[pos].element = (reprap_job_t *) malloc(sizeof(reprap_job_t));
                    }
                    ((reprap_job_t *) reprap_jobs[pos].element)->name = malloc(
                            strlen(cJSON_GetObjectItem(iterator, "name")->valuestring) + 1);
                    ((reprap_job_t *) reprap_jobs[pos].element)->dir = malloc(strlen(dir_name->valuestring) + 1);
                    ((reprap_job_t *) reprap_jobs[pos].element)->last_mod = malloc(1 + 1);
                    ((reprap_job_t *) reprap_jobs[pos].element)->generator = malloc(1 + 1);
                    strcpy(((reprap_job_t *) reprap_jobs[pos].element)->name,
                           cJSON_GetObjectItem(iterator, "name")->valuestring);
                    strcpy(((reprap_job_t *) reprap_jobs[pos].element)->dir, dir_name->valuestring);
                    if (strncmp("f", cJSON_GetObjectItem(iterator, "type")->valuestring, 1) == 0) {
                        reprap_jobs[pos].type = TREE_FILE_ELEM;
                    } else {
                        reprap_jobs[pos].type = TREE_FOLDER_ELEM;
                    }
                    pos++;
                }
            }
        }
        if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 100) == pdTRUE) {
            update_job_list_ui();
            xSemaphoreGive(xGuiSemaphore);
        }
    }
    cJSON_Delete(root);
}

void _process_reprap_fileinfo(char *data_buff) {
    cJSON *root = cJSON_Parse(data_buff);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s\n", error_ptr);
        }
        cJSON_Delete(root);
        return;
    }
    cJSON *err_resp = cJSON_GetObjectItem(root, DUET_ERR);
    if (err_resp && err_resp->valueint != 0) {     // maybe no active print
        cJSON_Delete(root);
        return;
    }
    cJSON *job_time_sim = cJSON_GetObjectItem(root, REPRAP_SIMTIME);
    if (job_time_sim && cJSON_IsNumber(job_time_sim)) {
        reprap_job_time_sim = job_time_sim->valueint;
    } else {
        reprap_job_time_sim = -1;
    }

    cJSON *job_print_time = cJSON_GetObjectItem(root, REPRAP_PRINTTIME);
    if (job_print_time && cJSON_IsNumber(job_print_time)) {
        reprap_job_time_file = job_print_time->valueint;
    }

    cJSON *job_name = cJSON_GetObjectItem(root, "fileName");
    if (job_name && cJSON_IsString(job_name)) {
        strncpy(current_job_name, &job_name->valuestring[10], MAX_FILA_NAME_LEN);
    }

    cJSON *job_height = cJSON_GetObjectItem(root, "height");
    if (job_height && cJSON_IsNumber(job_height)) {
        reprap_job_height = job_height->valuedouble;
    }
    cJSON *job_first_layer_height = cJSON_GetObjectItem(root, "firstLayerHeight");
    if (job_first_layer_height && cJSON_IsNumber(job_first_layer_height)) {
        reprap_job_first_layer_height = job_first_layer_height->valuedouble;
    }
    cJSON *job_layer_height = cJSON_GetObjectItem(root, "layerHeight");
    if (job_layer_height && cJSON_IsNumber(job_layer_height)) {
        reprap_job_layer_height = job_layer_height->valuedouble;
    }
    cJSON_Delete(root);
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    if (esp_http_client_get_status_code(evt->client) == 401) {
        ESP_LOGW(TAG, "Need to authorise first. Ignoring data.");
        return ESP_OK;
    }
    response_buff_t *resp_buff = (response_buff_t *) evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "Event handler detected http error");
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
                if ((resp_buff->buf_pos + evt->data_len) < JSON_BUFF_SIZE) {
                    strncpy(&resp_buff->buffer[resp_buff->buf_pos], (char *) evt->data, evt->data_len);
                    resp_buff->buf_pos += evt->data_len;
                } else {
                    ESP_LOGE(TAG, "Status-JSON buffer overflow (%i >= %i). Resetting!",
                             (evt->data_len + resp_buff->buf_pos), JSON_BUFF_SIZE);
                    resp_buff->buf_pos = 0;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            // ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            resp_buff->buf_pos = 0;
            break;
    }
    return ESP_OK;
}

void wifi_duet_authorise(response_buff_t *buffer, bool get_d2wc_config) {
    char printer_url[MAX_REQ_ADDR_LENGTH];
    sprintf(printer_url, "%s/rr_connect?password=%s", rep_addr, rep_pass);
    esp_http_client_config_t config = {
            .url = printer_url,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = _http_event_handle,
            .user_data = buffer
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

void wifi_duet_get_status(response_buff_t *resp_buff, int type) {
    ESP_LOGI(TAG, "Getting status %i", type);
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_status?type=%i", rep_addr, type);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = _http_event_handle,
            .user_data = resp_buff,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        switch (esp_http_client_get_status_code(client)) {
            case 200:
                status_request_err_cnt = 0;
                reppanel_conn_status = REPPANEL_WIFI_CONNECTED;
                _process_reprap_status(resp_buff->buffer, type);
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(resp_buff, true);
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error requesting RepRap status: %s", esp_err_to_name(err));
        status_request_err_cnt++;
        if (status_request_err_cnt > 0) {
            reppanel_conn_status = REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED;
            if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
                update_rep_panel_conn_status();
                xSemaphoreGive(xGuiSemaphore);
            }
        }
    }
    esp_http_client_cleanup(client);
}

bool reprap_wifi_send_gcode(char *gcode) {
    bool success = false;
    char request_addr[MAX_REQ_ADDR_LENGTH];
    char encoded_gcode[strlen(gcode) * 3];
    url_encode((unsigned char *) gcode, encoded_gcode);
    sprintf(request_addr, "%s/rr_gcode?gcode=%s", rep_addr, encoded_gcode);
    ESP_LOGV(TAG, "%s", request_addr);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = _http_event_handle,
            .user_data = &resp_buff_gui_task
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        switch (esp_http_client_get_status_code(client)) {
            case 200:
                // TODO check for rreply();
                success = true;
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(&resp_buff_gui_task, false);
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error sending GCode via WiFi: %s", esp_err_to_name(err));
        success = false;
    }
    esp_http_client_cleanup(client);
    return success;
}

void reprap_wifi_get_filelist(response_buff_t *resp_buffer, char *directory) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    char encoded_dir[strlen(directory) * 3];
    url_encode((unsigned char *) directory, encoded_dir);
    sprintf(request_addr, "%s/rr_filelist?dir=%s", rep_addr, encoded_dir);
    ESP_LOGI(TAG, "%s", request_addr);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = _http_event_handle,
            .user_data = resp_buffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                _process_reprap_filelist(resp_buffer->buffer);
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(resp_buffer, false);
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error getting file list via WiFi: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

/**
 * Task that gets files list
 * @param params Char array describing path to directory on pritner
 */
void reprap_wifi_get_filelist_task(void *params) {
    char *directory = params;
    char request_addr[MAX_REQ_ADDR_LENGTH];
    char encoded_dir[strlen(directory) * 3];
    url_encode((unsigned char *) directory, encoded_dir);
    sprintf(request_addr, "%s/rr_filelist?dir=%s", rep_addr, encoded_dir);
    ESP_LOGI("FileListTask", "Unformatted: %s", directory);
    ESP_LOGI("FileListTask", "Request: %s", request_addr);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = _http_event_handle,
            .user_data = &resp_buff_filelist_task,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                _process_reprap_filelist(resp_buff_filelist_task.buffer);
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(&resp_buff_filelist_task, false);
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error getting file list via WiFi: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

void reprap_wifi_get_fileinfo(response_buff_t *resp_data, char *filename) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    if (filename != NULL) {
        char encoded_filename[strlen(filename) * 3];
        url_encode((unsigned char *) filename, encoded_filename);
        sprintf(request_addr, "%s/rr_fileinfo?dir=%s", rep_addr, encoded_filename);
    } else {
        sprintf(request_addr, "%s/rr_fileinfo", rep_addr);
    }
    ESP_LOGI(TAG, "Getting file info %s", request_addr);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = _http_event_handle,
            .user_data = resp_data,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
//        ESP_LOGI(TAG, "Status = %d, content_length = %d",
//                 esp_http_client_get_status_code(client),
//                 esp_http_client_get_content_length(client));

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                _process_reprap_fileinfo(resp_data->buffer);
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(resp_data, false);
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error getting file info via WiFi: %s", esp_err_to_name(err));
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
            .user_data = &resp_buff_gui_task,
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
            wifi_duet_authorise(&resp_buff_gui_task, false);
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}

void reprap_wifi_download(response_buff_t *response_buffer, char *file) {
    ESP_LOGI(TAG, "Downloading %s", file);
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_download?name=%s", rep_addr, file);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = _http_event_handle,
            .user_data = response_buffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 100) == pdTRUE) {
                    _process_duet_settings(response_buffer->buffer);
                    xSemaphoreGive(xGuiSemaphore);
                }
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(response_buffer, false);
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error requesting RepRap status: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

/**
 * Send GCode to the printer. Blocking call. Call from UI thread!
 * @param gcode_command
 */
bool reprap_send_gcode(char *gcode_command) {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        if (reprap_wifi_send_gcode(gcode_command)) {
            add_console_hist_entry(gcode_command, "", CONSOLE_TYPE_REPPANEL);
            update_entries_ui();
            return true;
        }
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Writing to UART not supported for now");
    }
}

void request_filaments() {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting filaments");
        reprap_wifi_get_filelist(&resp_buff_gui_task, "0:/filaments&first=0");
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Using UART not supported for now");
    }
}

/**
 * Launches a new thread that requests macros. Updates Macros list in GUI on success. Non blocking call.
 * @param folder_path Can be empty in case root macro folder content is requested
 */
void request_macros_async(char *folder_path) {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting macros");
        TaskHandle_t get_filelist_async_task_handle = NULL;
        xTaskCreate(reprap_wifi_get_filelist_task, "macros request task", 1024 * 3, folder_path,
                    tskIDLE_PRIORITY, &get_filelist_async_task_handle);
        configASSERT(get_filelist_async_task_handle);
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Using UART not supported for now");
    }
}

void request_macros() {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting macros");
        reprap_wifi_get_filelist(&resp_buff_gui_task, "0:/macros&first=0");
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Using UART not supported for now");
    }
}

/**
 * Updates internal global variables with file info
 * @param file_name Path to file name on printer local storage. NULL in case you need file info of currently printed file
 */
void request_fileinfo(char *file_name) {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting file info");
        reprap_wifi_get_fileinfo(&resp_buff_gui_task, file_name);
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Using UART not supported for now");
    }
}

/**
 * Launches a new thread that requests job list. Updates Job list in GUI on success. Non blocking call.
 */
void request_jobs_async(char *folder_path) {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting jobs");
        TaskHandle_t get_filelist_async_task_handle = NULL;
        xTaskCreate(reprap_wifi_get_filelist_task, "jobs request task", 1024 * 3, folder_path,
                    tskIDLE_PRIORITY, &get_filelist_async_task_handle);
        configASSERT(get_filelist_async_task_handle);
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Using UART not supported for now");
    }
}

void request_jobs() {
    if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting jobs");
        reprap_wifi_get_filelist(&resp_buff_gui_task, "0:/gcodes&first=0");
    } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
        ESP_LOGW(TAG, "Using UART not supported for now");
    }
}

/**
 * Called every 750ms
 * @param task
 */
void request_reprap_status_updates(void *params) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = (500 / portTICK_PERIOD_MS);
    xLastWakeTime = xTaskGetTickCount();
    int i = 8;
    UBaseType_t uxHighWaterMark;
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        if (reppanel_conn_status == REPPANEL_WIFI_CONNECTED ||
            reppanel_conn_status == REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED) {
            if (!got_duet_settings)
                reprap_wifi_download(&resp_buff_status_update_task, "0%3A%2Fsys%2Fdwc2settings.json");
            if (!got_filaments) request_filaments();
            if (!got_status_two) wifi_duet_get_status(&resp_buff_status_update_task, 2);
            if (!job_running)
                wifi_duet_get_status(&resp_buff_status_update_task, 1);
            else
                wifi_duet_get_status(&resp_buff_status_update_task, 3);
            if (i % 20 == 0) {
                wifi_duet_get_status(&resp_buff_status_update_task, 2);
                i = 0;
            } else {
                i++;
            }
        } else if (reppanel_conn_status == REPPANEL_UART_CONNECTED) {
            ESP_LOGW(TAG, "Using UART not supported for now");
        }
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "%i free bytes", uxHighWaterMark * 4);
    }
    vTaskDelete(NULL);
}
