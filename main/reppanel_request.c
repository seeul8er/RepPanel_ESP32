//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <lvgl/src/lv_misc/lv_task.h>
#include <lwip/ip4_addr.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <lvgl/lvgl.h>
#include <esp_log.h>
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
#include "esp32_uart.h"
#include "esp32_wifi.h"
#include "rrf3_object_model_parser.h"
#include "rrf_objects.h"

#define TAG                 "RequestTask"
#define REQUEST_TIMEOUT_MS  150

file_tree_elem_t reprap_dir_elem[MAX_NUM_ELEM_DIR];    // put it to the external PSRAM

char rep_addr_resolved[256];

static bool got_filaments = false;
static bool got_extended_status = false;
static bool got_duet_settings = false;
static bool duet_request_macros = false;
static bool duet_request_jobs = false;
static bool duet_request_reply = false;
static int status_request_err_cnt = 0;      // request errors in a row
static bool duet_sbc_mode = false;   // false=Standalone, true=SBC
bool job_paused = false;
int seq_num_msgbox = 0;
int last_status_seq = -1;

static bool uart_request_file_info = false;
static char request_file_path[512];

const char *decode_reprap2_status(const char *valuestring) {
    job_paused = false;
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
        case REPRAP_STATUS_OFF:
            job_running = false;
            return "Off";
        default:
            break;
    }
    return "UnknownStatus";
}

void decode_rrf3_status() {
    if (strncmp(reprap_model.reprap_state.status, "simulating", REPRAP_MAX_STATUS_LEN - 1) == 0
        || strncmp(reprap_model.reprap_state.status, "printing", REPRAP_MAX_STATUS_LEN - 1) == 0
        || strncmp(reprap_model.reprap_state.status, "processing", REPRAP_MAX_STATUS_LEN - 1) == 0) {
        job_running = true;
        job_paused = false;
    } else if (strncmp(reprap_model.reprap_state.status, "paused", REPRAP_MAX_STATUS_LEN - 1) == 0) {
        job_paused = true;
        job_running = true;
    } else {
        job_running = false;
        job_paused = false;
    }
}

/**
 * For legacy RRF2 status responses
 * @param buff raw HTTP response buffer containing the JSON
 */
void process_reprap2_status(char *buff) {
    cJSON *root = cJSON_Parse(buff);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s", error_ptr);
        }
        cJSON_Delete(root);
        return;
    }
    cJSON *name = cJSON_GetObjectItem(root, DUET_STATUS);
    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
        strlcpy(reprap_model.reprap_state.status, decode_reprap2_status(name->valuestring), REPRAP_MAX_STATUS_LEN);
    }

    cJSON *coords = cJSON_GetObjectItem(root, "coords");
    if (coords) {
        cJSON *axesHomed = cJSON_GetObjectItem(coords, "axesHomed");
        if (axesHomed && cJSON_IsArray(axesHomed)) {
            reprap_axes.homed[0] = cJSON_GetArrayItem(axesHomed, 0)->valueint == 1;
            reprap_axes.homed[1] = cJSON_GetArrayItem(axesHomed, 1)->valueint == 1;
            reprap_axes.homed[2] = cJSON_GetArrayItem(axesHomed, 2)->valueint == 1;
        }
        cJSON *xyz = cJSON_GetObjectItem(coords, "xyz");
        if (xyz && cJSON_IsArray(xyz)) {
            reprap_axes.axes[0] = cJSON_GetArrayItem(xyz, 0)->valuedouble;
            reprap_axes.axes[1] = cJSON_GetArrayItem(xyz, 1)->valuedouble;
            reprap_axes.axes[2] = cJSON_GetArrayItem(xyz, 2)->valuedouble;
        }
    }

    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (params) {
        cJSON *atxPower = cJSON_GetObjectItem(params, "atxPower");
        if (atxPower && cJSON_IsNumber(atxPower)) {
            reprap_params.power = atxPower->valueint == 1;
        }
        cJSON *fanPercent = cJSON_GetObjectItem(params, "fanPercent");
        if (fanPercent && cJSON_IsArray(fanPercent)) {
            reprap_params.fan = cJSON_GetArrayItem(fanPercent, 0)->valueint;
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
        cJSON *duet_temps_bed_heater = cJSON_GetObjectItem(duet_temps_bed,
                                                           DUET_TEMPS_BED_HEATER);    // bed heater state
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

    bool disp_msg = false;      // Message without title
    bool disp_msgbox = false;   // Message box with title
    bool disp_h_msgbox = false; // height adjust dialog
    static char msg_title[384];
    static char msg_msg[384];
    static char msg_txt[384];
    cJSON *duet_seq = cJSON_GetObjectItem(root, "seq");
    cJSON *duet_output = cJSON_GetObjectItem(root, "output");
    if (duet_output) {
        cJSON *duet_output_msg = cJSON_GetObjectItem(duet_output, "message");
        if (duet_output_msg && cJSON_IsString(duet_output_msg) && duet_seq && duet_seq->valueint != last_status_seq) {
            disp_msg = true;
            strncpy(msg_txt, duet_output_msg->valuestring, 384);
        }
        // Right now we only have a msg box for manual bed calibration
        cJSON *duet_output_msgbox = cJSON_GetObjectItem(duet_output, "msgBox");
        if (duet_output_msgbox) {
            cJSON *seq = cJSON_GetObjectItem(duet_output_msgbox, "seq");
            cJSON *title = cJSON_GetObjectItem(duet_output_msgbox, "title");
            cJSON *duet_msg = cJSON_GetObjectItem(duet_output_msgbox, "msg");
            strncpy(msg_title, title->valuestring, 384);
            strncpy(msg_msg, duet_msg->valuestring, 384);
            // Beware. This is dirty. Check if we want to show this msg box. We might already display it
            if (seq->valueint != seq_num_msgbox) {
                seq_num_msgbox = seq->valueint;
                if (strcmp(duet_msg->valuestring,
                           "Adjust height until the nozzle just touches the bed, then press OK") == 0)
                    disp_h_msgbox = true;
                else
                    disp_msgbox = true;
            }
        }
    }

    if (duet_seq && cJSON_IsNumber(duet_seq)) {
        if (duet_seq->valueint != last_status_seq) {
            duet_request_reply = true;
            ESP_LOGI(TAG, "Need reply!");
            // When connected via UART the reply is already part of the msg
            cJSON *duet_resp = cJSON_GetObjectItem(root, "resp");
            if (duet_resp && strlen(duet_resp->valuestring) > 0 &&
                duet_resp->valuestring[0] != '\n') {      // sometimes it's just a new line char
                //ESP_LOGI(TAG, "Length MSG: %llu - %s", strlen(duet_resp->valuestring), duet_resp->valuestring);
                disp_msg = true;
                strncpy(msg_txt, duet_resp->valuestring, 384);
            }
        }
        last_status_seq = duet_seq->valueint;
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
    reprap_model.num_heaters = pos;

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
                reprap_tools[pos].heater_indx = pos + 1;    // set to some default value
                if (cJSON_IsArray(cJSON_GetObjectItem(iterator, "heaters"))) {
                    // Ignore multiple heaters per tool
                    cJSON *heaterindx_item = cJSON_GetArrayItem(cJSON_GetObjectItem(iterator, "heaters"), 0);
                    reprap_tools[pos].heater_indx = heaterindx_item->valueint;
                }
                pos++;
            }
        }
        got_extended_status = true;
        reprap_model.num_tools = pos;    // update number of tools
    }

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

    // Get current tool temperatures
    cJSON *duet_temps_current = cJSON_GetObjectItem(duet_temps, DUET_TEMPS_CURRENT);
    if (duet_temps_current) {
        for (int i = 0; i < reprap_model.num_tools; i++) {
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
        for (int i = 0; i < reprap_model.num_tools; i++) {
            cJSON *tool_active_temps_arr = cJSON_GetArrayItem(duet_temps_tools_active,
                                                              reprap_tools[i].number);
            cJSON *tool_standby_temps_arr = cJSON_GetArrayItem(duet_temps_tools_standby,
                                                               reprap_tools[i].number);
            if (tool_active_temps_arr) {
                reprap_tools[i].active_temp = cJSON_GetArrayItem(tool_active_temps_arr, 0)->valuedouble;
            }
            if (tool_standby_temps_arr) {
                reprap_tools[i].standby_temp = cJSON_GetArrayItem(tool_standby_temps_arr, 0)->valuedouble;
            }
        }
    }
    // print job status
    bool got_printjob_status = false;
    cJSON *print_progess = cJSON_GetObjectItem(root, REPRAP_FRAC_PRINTED);
    if (print_progess && cJSON_IsNumber(print_progess)) {
        got_printjob_status = true;
        reprap_job_percent = print_progess->valuedouble;
    }

    cJSON *job_dur = cJSON_GetObjectItem(root, REPRAP_JOB_DUR);
    if (job_dur && cJSON_IsNumber(job_dur)) {
        reprap_model.reprap_job.duration = job_dur->valueint;
    }

    cJSON *job_curr_layer = cJSON_GetObjectItem(root, REPRAP_CURR_LAYER);
    if (job_curr_layer && cJSON_IsNumber(job_curr_layer)) {
        reprap_model.reprap_job.layer = job_curr_layer->valueint;
    }

    // update UI
    if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 100) == pdTRUE) {
        if (label_status != NULL) lv_label_set_text(label_status, reprap_model.reprap_state.status);
        update_ui_machine();
        update_bed_temps_ui();  // update UI with new values
        update_heater_status_ui(_heater_states, reprap_model.num_heaters);  // update UI with new values
        update_current_tool_temps_ui();     // update UI with new values
        if (got_extended_status && label_extruder_name != NULL) {
            lv_label_set_text(label_extruder_name, reprap_tools[current_visible_tool_indx].name);
        }
        if (got_printjob_status) update_print_job_status_ui();
        if (disp_msg) show_reprap_dialog("", msg_txt, (const uint8_t *) 1, false);
        if (disp_h_msgbox) show_reprap_dialog("Adjust Z", "", (const uint8_t *) 2, true);
        if (disp_msgbox) show_reprap_dialog(msg_title, msg_msg, (const uint8_t *) 2, false);
        update_rep_panel_conn_status();
        xSemaphoreGive(xGuiSemaphore);
    }

    cJSON_Delete(root);

}

/**
 * For RRF3 object model responses
 * @param buff raw HTTP response buffer containing object model JSON
 */
void process_reprap3_status(char *buff) {
    cJSON *root = cJSON_Parse(buff);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s", error_ptr);
        }
        cJSON_Delete(root);
        return;
    }
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (result == NULL) {
        cJSON_Delete(root);
        return;
    }
    cJSON *key = cJSON_GetObjectItem(root, "key");
    cJSON *flags = cJSON_GetObjectItem(root, "flags");
    cJSON *sub_object_result;
    sub_object_result = cJSON_GetObjectItem(result, "boards");
    if (sub_object_result)
        reppanel_parse_rrf_boards(sub_object_result, flags, &reprap_model);
    else if (strcmp(key->valuestring, "boards") == 0)
        reppanel_parse_rrf_boards(result, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "fans");
    if (sub_object_result)
        reppanel_parse_rrf_fans(sub_object_result, flags, &reprap_model);
    else if (strcmp(key->valuestring, "fans") == 0) {
        reppanel_parse_rrf_fans(result, flags, &reprap_model);
    }

    sub_object_result = cJSON_GetObjectItem(result, "heat");
    if (sub_object_result)
        reppanel_parse_rrf_heaters(sub_object_result, heater_states, flags, &reprap_model);
    else if (strcmp(key->valuestring, "heat") == 0)
        reppanel_parse_rrf_heaters(result, heater_states, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "tools");
    if (sub_object_result)
        reppanel_parse_rrf_tools(sub_object_result, heater_states, flags, &reprap_model);
    else if (strcmp(key->valuestring, "tools") == 0)
        reppanel_parse_rrf_tools(result, heater_states, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "job");
    if (sub_object_result)
        reppanel_parse_rrf_job(sub_object_result, flags, &reprap_model);
    else if (strcmp(key->valuestring, "job") == 0)
        reppanel_parse_rrf_job(result, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "move");
    if (sub_object_result)
        reppanel_parse_rrf_move(sub_object_result, flags, &reprap_model);
    else if (strcmp(key->valuestring, "move") == 0)
        reppanel_parse_rrf_move(result, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "state");
    if (sub_object_result)
        reppanel_parse_rrf_state(sub_object_result, flags, &reprap_model);
    else if (strcmp(key->valuestring, "state") == 0)
        reppanel_parse_rrf_state(result, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "network");
    if (sub_object_result)
        reppanel_parse_rrf_network(sub_object_result, flags, &reprap_model);
    else if (strcmp(key->valuestring, "network") == 0)
        reppanel_parse_rrf_network(result, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "sensors");
    if (sub_object_result)
        reppanel_parse_rrf_sensors(sub_object_result, flags, &reprap_model);
    else if (strcmp(key->valuestring, "sensors") == 0)
        reppanel_parse_rrf_sensors(result, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "inputs");
    if (sub_object_result)
        reppanel_parse_rrf_inputs(sub_object_result, flags, &reprap_model);
    else if (strcmp(key->valuestring, "inputs") == 0)
        reppanel_parse_rrf_inputs(result, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "seqs");
    if (sub_object_result) {
        reppanel_parse_rrf_seqs(sub_object_result, &reprap_model);
    } else if (strcmp(key->valuestring, "seqs") == 0) {
        reppanel_parse_rrf_seqs(result, &reprap_model);
    }

    sub_object_result = cJSON_GetObjectItem(result, "global");
    if (sub_object_result)
        reppanel_parse_rrf_global(sub_object_result, flags, &reprap_model);
    else if (strcmp(key->valuestring, "global") == 0)
        reppanel_parse_rrf_global(result, flags, &reprap_model);

    sub_object_result = cJSON_GetObjectItem(result, "directories");
    if (sub_object_result) {
        reppanel_parse_rrf_directories(sub_object_result, flags, &reprap_model);
    } else if (strcmp(key->valuestring, "directories") == 0) {
        reppanel_parse_rrf_directories(result, flags, &reprap_model);
    }
    cJSON_Delete(root);

    decode_rrf3_status();

    if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 100) == pdTRUE) {
        if (label_status != NULL) lv_label_set_text(label_status, reprap_model.reprap_state.status);
        update_ui_machine();
        update_bed_temps_ui();  // update UI with new values
        update_heater_status_ui(heater_states, reprap_model.num_heaters);  // update UI with new values
        update_current_tool_temps_ui();     // update UI with new values
        if (got_extended_status && label_extruder_name != NULL) {
            lv_label_set_text(label_extruder_name, reprap_tools[current_visible_tool_indx].name);
        }
        if (job_running) update_print_job_status_ui();
        update_rep_panel_conn_status();
        if (reprap_model.reprap_state.new_msg) {
            show_reprap_dialog(reprap_model.reprap_state.msg_box_title, reprap_model.reprap_state.msg_box_msg,
                               &reprap_model.reprap_state.mode, reprap_model.reprap_state.show_axis_controls);
            reprap_model.reprap_state.new_msg = false;
        }
        xSemaphoreGive(xGuiSemaphore);
    }
}

void process_reprap_status(char *buff) {
    if (reprap_model.api_level < 1)
        process_reprap2_status(buff);
    else
        process_reprap3_status(buff);
}

void process_reprap_settings(char *buff) {
    ESP_LOGI(TAG, "Processing D2WC status json");
    cJSON *root = cJSON_Parse(buff);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s", error_ptr);
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
    ESP_LOGI(TAG, "Got D2WC status json");
    got_duet_settings = true;
    cJSON_Delete(root);
}

void process_reprap_filelist(char *buffer) {
    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Error before: %s", error_ptr);
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
    if (dir_name && strncmp("0:/filaments", dir_name->valuestring, 12) == 0) {
        ESP_LOGI(TAG, "Processing filament names");
        got_filaments = true;

        cJSON *filament_folders = cJSON_GetObjectItem(root, "files");
        if (filament_folders) {
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
        } else {
            filament_names[0] = '\0';
        }
    } else if (dir_name && strncmp("0:/macros", dir_name->valuestring, 9) == 0) {
        ESP_LOGI(TAG, "Processing macros");
        for (int i = 0; i < MAX_NUM_ELEM_DIR; i++) {
            reprap_dir_elem[i].type = TREE_EMPTY_ELEM;
        }
        cJSON *_folders = cJSON_GetObjectItem(root, "files");
        if (_folders) {
            cJSON *iterator = NULL;
            int pos = 0;
            cJSON_ArrayForEach(iterator, _folders) {
                if (cJSON_IsObject(iterator)) {
                    if (pos < MAX_NUM_ELEM_DIR) {
                        strncpy(reprap_dir_elem[pos].name, cJSON_GetObjectItem(iterator, "name")->valuestring,
                                MAX_LEN_FILENAME - 1);
                        strncpy(reprap_dir_elem[pos].dir, dir_name->valuestring, MAX_LEN_DIRNAME - 1);
                        reprap_dir_elem[pos].time_stamp = datestr_2unix(
                                cJSON_GetObjectItem(iterator, "date")->valuestring);
                        if (strncmp("f", cJSON_GetObjectItem(iterator, "type")->valuestring, 1) == 0) {
                            reprap_dir_elem[pos].type = TREE_FILE_ELEM;
                        } else {
                            reprap_dir_elem[pos].type = TREE_FOLDER_ELEM;
                        }
                        pos++;
                    }
                }
            }
        }
        if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 100) == pdTRUE) {
            update_macro_list_ui();
            xSemaphoreGive(xGuiSemaphore);
        }
    } else if (dir_name && strncmp("0:/gcodes", dir_name->valuestring, 9) == 0) {
        ESP_LOGI(TAG, "Processing jobs");
        cJSON *iterator = NULL;
        for (int i = 0; i < MAX_NUM_ELEM_DIR; i++) {  // clear array
            reprap_dir_elem[i].type = TREE_EMPTY_ELEM;
        }
        int pos = 0;
        cJSON *_folders = cJSON_GetObjectItem(root, "files");
        if (_folders) {
            cJSON_ArrayForEach(iterator, _folders) {
                if (cJSON_IsObject(iterator)) {
                    if (pos < MAX_NUM_ELEM_DIR) {
                        strncpy(reprap_dir_elem[pos].name, cJSON_GetObjectItem(iterator, "name")->valuestring,
                                MAX_LEN_FILENAME - 1);
                        strncpy(reprap_dir_elem[pos].dir, dir_name->valuestring, MAX_LEN_DIRNAME - 1);
                        reprap_dir_elem[pos].time_stamp = datestr_2unix(
                                cJSON_GetObjectItem(iterator, "date")->valuestring);
                        if (strncmp("f", cJSON_GetObjectItem(iterator, "type")->valuestring, 1) == 0) {
                            reprap_dir_elem[pos].type = TREE_FILE_ELEM;
                        } else {
                            reprap_dir_elem[pos].type = TREE_FOLDER_ELEM;
                        }
                        pos++;
                    }
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

void process_reprap_fileinfo(char *data_buff) {
    cJSON *root = cJSON_Parse(data_buff);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "Got %s", data_buff);
            ESP_LOGE(TAG, "Error before: %s", error_ptr);
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
        reprap_model.reprap_job.file.simulatedTime = job_time_sim->valueint;
    } else {
        reprap_model.reprap_job.file.simulatedTime = -1;
    }

    cJSON *job_print_time = cJSON_GetObjectItem(root, REPRAP_PRINTTIME);
    if (job_print_time && cJSON_IsNumber(job_print_time)) {
        reprap_model.reprap_job.file.printTime = job_print_time->valueint;
    }

    cJSON *job_name = cJSON_GetObjectItem(root, "fileName");
    if (job_name && cJSON_IsString(job_name)) {
        strncpy(reprap_model.reprap_job.file.fileName, &job_name->valuestring[10], MAX_LEN_FILENAME);
    }

    cJSON *job_height = cJSON_GetObjectItem(root, "height");
    if (job_height && cJSON_IsNumber(job_height)) {
        reprap_model.reprap_job.file.height = (float) job_height->valuedouble;
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

void process_reprap_reply(wifi_response_buff_t *response_buffer) {
    if (response_buffer->buf_pos > 1) {
        if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
            show_reprap_dialog("Response to G-Code", response_buffer->buffer, (const uint8_t *) 1, false);
            xSemaphoreGive(xGuiSemaphore);
        }
        reprap_model.reprap_seqs_changed.reply_changed = 0;
    }
}

void reprap_uart_send_gcode(char *gcode) {
    reppanel_write_uart(gcode, strlen(gcode));
    ESP_LOGD(TAG, "Sent %s", gcode);
}

void reprap_uart_get_status(uart_response_buff_t *receive_buff, int type, char *key, char *flags) {
    ESP_LOGI(TAG, "Getting status (UART) %i", type);
    char buff[32];
    if (reprap_model.api_level < 1) {
        sprintf(buff, "M408 S%i", type);
    } else {
        sprintf(buff, "M409 K\"%s\" F\"%s\"", key, flags);
    }
    reprap_uart_send_gcode(buff);
    if (reppanel_read_response(receive_buff)) {
        process_reprap_status((char *) receive_buff->buffer);
    }
}

void reprap_uart_get_file_info(uart_response_buff_t *receive_buff) {
    char buff[524];
    sprintf(buff, "M36 \"%s\"", request_file_path);
    reprap_uart_send_gcode(buff);
    if (reppanel_read_response(receive_buff)) {
        process_reprap_fileinfo((char *) receive_buff->buffer);
        uart_request_file_info = false;
    }
}

void reprap_uart_get_filelist(uart_response_buff_t *receive_buff, char *path) {
    char buff[512];
    sprintf(buff, "M20 S3 P\"%s\"", path);
    reprap_uart_send_gcode(buff);
    if (reppanel_read_response(receive_buff)) {
        process_reprap_filelist((char *) receive_buff->buffer);
    }
}

/**
 * Fill internals with dummy values since we can not download files using UART ?!
 */
void reprap_uart_download(uart_response_buff_t *receive_buff, char *path) {
    ESP_LOGI(TAG, "Setting hardcoded values for bed/tool temperatures");
    // max len NUM_TEMPS_BUFF, last must be <0
    static double bed_temps_hardcoded[] = {0, 40, 53, 55, 60, 70, 80, 90, 100, 105, 110, -1};
    static double tool_temps_hardcoded[] = {0, 160, 190, 195, 200, 205, 210, 230, 235, 240, 270, 280, -1};
    memcpy(reprap_bed_poss_temps.temps_standby, bed_temps_hardcoded, sizeof(bed_temps_hardcoded));
    memcpy(reprap_bed_poss_temps.temps_active, bed_temps_hardcoded, sizeof(bed_temps_hardcoded));
    memcpy(reprap_tool_poss_temps.temps_standby, tool_temps_hardcoded, sizeof(tool_temps_hardcoded));
    memcpy(reprap_tool_poss_temps.temps_active, tool_temps_hardcoded, sizeof(tool_temps_hardcoded));
    got_duet_settings = true;
}

esp_err_t http_event_handle(esp_http_client_event_t *evt) {
    if (esp_http_client_get_status_code(evt->client) == 401) {
        ESP_LOGW(TAG, "Need to authorise first. Ignoring data.");
        return ESP_OK;
    }
    wifi_response_buff_t *resp_buff = (wifi_response_buff_t *) evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "Event handler detected http error");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            resp_buff->buf_pos = 0;
            break;
        case HTTP_EVENT_HEADER_SENT:
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
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
            break;
        case HTTP_EVENT_DISCONNECTED:
            resp_buff->buf_pos = 0;
            break;
    }
    return ESP_OK;
}

void wifi_duet_authorise(wifi_response_buff_t *resp_buff, bool get_d2wc_config) {
    char printer_url[MAX_REQ_ADDR_LENGTH];
    if (duet_sbc_mode) {
        sprintf(printer_url, "%s/machine/connect?password=%s", rep_addr_resolved, rep_pass);
    } else {
        sprintf(printer_url, "%s/rr_connect?password=%s", rep_addr_resolved, rep_pass);
    }
    esp_http_client_config_t config = {
            .url = printer_url,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = http_event_handle,
            .user_data = resp_buff
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        switch (esp_http_client_get_status_code(client)) {
            case 200:
                status_request_err_cnt = 0;
                if (rp_conn_stat != REPPANEL_UART_CONNECTED)
                    rp_conn_stat = REPPANEL_WIFI_CONNECTED;
                cJSON *root = cJSON_Parse(resp_buff->buffer);   // get JSON response to read API level
                if (root == NULL) {
                    ESP_LOGE(TAG, "Error parsing authorisation response");
                    cJSON_Delete(root);
                    return;
                }
                reppanel_parse_rr_connect(root, &reprap_model);
                cJSON_Delete(root);
                ESP_LOGI(TAG, "Detected API Level %i", reprap_model.api_level);
                break;
            case 500:
                ESP_LOGE(TAG, "Generic error authorising DUET");
                break;
            case 502:
                ESP_LOGE(TAG, "Incompatible DCS version");
                break;
            case 503:
                ESP_LOGE(TAG, "Authorize: DCS is unavailable");
                duet_sbc_mode = false;
                break;
            default:
                break;
        }
    }
    esp_http_client_cleanup(client);
}

void reprap_wifi_get_status(wifi_response_buff_t *resp_buff, int type, char *key, char *flags) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    if (duet_sbc_mode)
        sprintf(request_addr, "%s/machine/status", rep_addr_resolved);
    else {
        if (reprap_model.api_level < 1) {
            sprintf(request_addr, "%s/rr_status?type=%i", rep_addr_resolved, type);
        } else {
            if (strlen(key) > 0) {
                sprintf(request_addr, "%s/rr_model?key=%s&flags=%s", rep_addr_resolved, key, flags);
            } else {
                sprintf(request_addr, "%s/rr_model?flags=%s", rep_addr_resolved, flags);
            }
        }
    }
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = http_event_handle,
            .user_data = resp_buff,
    };
    ESP_LOGI(TAG, "Requesting: %s", request_addr);
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        switch (esp_http_client_get_status_code(client)) {
            case 200:
                status_request_err_cnt = 0;
                if (rp_conn_stat != REPPANEL_UART_CONNECTED)
                    rp_conn_stat = REPPANEL_WIFI_CONNECTED;
                process_reprap_status(resp_buff->buffer);
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(resp_buff, true);
                break;
            case 500:
                ESP_LOGE(TAG, "Generic error getting status");
                break;
            case 502:
                ESP_LOGE(TAG, "Incompatible DCS version");
                break;
            case 503:
                ESP_LOGE(TAG, "Get Status: DCS is unavailable %i %s %s", type, key, flags);
                duet_sbc_mode = false;
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error requesting RepRap status: %s", esp_err_to_name(err));
        status_request_err_cnt++;
        if (status_request_err_cnt > 0) {
            if (rp_conn_stat != REPPANEL_UART_CONNECTED)
                rp_conn_stat = REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED;
            if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
                update_rep_panel_conn_status();
                xSemaphoreGive(xGuiSemaphore);
            }
        }
    }
    esp_http_client_cleanup(client);
}

void reprap_wifi_get_rreply(wifi_response_buff_t *response_buffer) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    sprintf(request_addr, "%s/rr_reply", rep_addr_resolved);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = 1000,
            .event_handler = http_event_handle,
            .user_data = response_buffer
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    ESP_LOGI(TAG, "Requesting rr_reply");
    if (err == ESP_OK) {
        switch (esp_http_client_get_status_code(client)) {
            case 200:
                if (response_buffer->buf_pos > 1) {
                    ESP_LOGI(TAG, "Got reply!");
                    process_reprap_reply(response_buffer);
                }
                break;
            case 401:
                ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(response_buffer, false);
                break;
            default:
                ESP_LOGE(TAG, "Error getting reply (HTTP error code %i)!", esp_http_client_get_status_code(client));
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error getting reply via WiFi: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

bool reprap_wifi_send_gcode(char *gcode) {
    bool success = false;
    char request_addr[MAX_REQ_ADDR_LENGTH];
    char encoded_gcode[strlen(gcode) * 3];
    url_encode((unsigned char *) gcode, encoded_gcode);
    if (duet_sbc_mode) {
        sprintf(request_addr, "%s/machine/code", rep_addr_resolved);
    } else {
        sprintf(request_addr, "%s/rr_gcode?gcode=%s", rep_addr_resolved, encoded_gcode);
    }

    ESP_LOGV(TAG, "%s", request_addr);
    wifi_response_buff_t resp_buff_gui_task;
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = http_event_handle,
            .user_data = &resp_buff_gui_task
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (duet_sbc_mode) {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_post_field(client, gcode, strlen(gcode));
    }
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d", esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        switch (esp_http_client_get_status_code(client)) {
            case 200:
                success = true;
                break;
            case 401:
                //ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(&resp_buff_gui_task, false);
                break;
            case 500:
                ESP_LOGE(TAG, "Generic error getting status");
                break;
            case 502:
                ESP_LOGE(TAG, "Incompatible DCS version");
                break;
            case 503:
                ESP_LOGE(TAG, "Send GCode: DCS is unavailable");
                duet_sbc_mode = false;
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Error sending GCode via WiFi: %s", esp_err_to_name(err));
        success = false;
    }
    esp_http_client_cleanup(client);
    if (success) {
        if (duet_sbc_mode) {
            // TODO: Get reply
        } else {
            reprap_wifi_get_rreply(&resp_buff_gui_task);
        }
    }
    return success;
}

void reprap_wifi_get_filelist(wifi_response_buff_t *resp_buffer, char *directory) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    char encoded_dir[strlen(directory) * 3];
    url_encode((unsigned char *) directory, encoded_dir);
    if (duet_sbc_mode) {
        sprintf(request_addr, "%s/machine/directory/%s", rep_addr_resolved, encoded_dir);
    } else {
        sprintf(request_addr, "%s/rr_filelist?dir=%s&first=0", rep_addr_resolved, encoded_dir);
    }
    ESP_LOGI(TAG, "%s", request_addr);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = http_event_handle,
            .user_data = resp_buffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Got file list via WiFi %d", esp_http_client_get_content_length(client));

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                process_reprap_filelist(resp_buffer->buffer);
                break;
            case 401:
                //ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(resp_buffer, false);
                break;
            case 500:
                ESP_LOGE(TAG, "Generic error getting file list");
                break;
            case 502:
                ESP_LOGE(TAG, "Incompatible DCS version");
                break;
            case 503:
                ESP_LOGE(TAG, "Get filelist: DCS is unavailable");
                duet_sbc_mode = false;
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
    if (duet_sbc_mode) {
        sprintf(request_addr, "%s/machine/directory/%s", rep_addr_resolved, encoded_dir);
    } else {
        sprintf(request_addr, "%s/rr_filelist?dir=%s&first=0", rep_addr_resolved, encoded_dir);
    }
    ESP_LOGD("FileListTask", "Unformatted: %s", directory);
    ESP_LOGD("FileListTask", "Request: %s", request_addr);
    wifi_response_buff_t resp_buff_filelist_task;
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = http_event_handle,
            .user_data = &resp_buff_filelist_task,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        //ESP_LOGI(TAG, "Status = %d, content_length = %d", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                process_reprap_filelist(resp_buff_filelist_task.buffer);
                break;
            case 401:
                //ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(&resp_buff_filelist_task, false);
                break;
            case 500:
                ESP_LOGE(TAG, "Generic error getting file list");
                break;
            case 502:
                ESP_LOGE(TAG, "Incompatible DCS version");
                break;
            case 503:
                ESP_LOGE(TAG, "Get filelist: DCS is unavailable");
                duet_sbc_mode = false;
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

void reprap_wifi_get_fileinfo(wifi_response_buff_t *resp_data, char *filename) {
    char request_addr[MAX_REQ_ADDR_LENGTH];
    if (filename != NULL) {
        char encoded_filename[strlen(filename) * 3];
        url_encode((unsigned char *) filename, encoded_filename);
        if (duet_sbc_mode) {
            sprintf(request_addr, "%s/machine/fileinfo/%s", rep_addr_resolved, encoded_filename);
        } else {
            sprintf(request_addr, "%s/rr_fileinfo?dir=%s", rep_addr_resolved, encoded_filename);
        }
    } else {
        if (duet_sbc_mode) {
            sprintf(request_addr, "%s/machine/fileinfo", rep_addr_resolved);
        } else {
            sprintf(request_addr, "%s/rr_fileinfo", rep_addr_resolved);
        }
    }
    ESP_LOGI(TAG, "Getting file info %s", request_addr);
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = http_event_handle,
            .user_data = resp_data,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        switch (esp_http_client_get_status_code(client)) {
            case 200:
                process_reprap_fileinfo(resp_data->buffer);
                break;
            case 401:
                //ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(resp_data, false);
                break;
            case 500:
                ESP_LOGE(TAG, "Generic error getting file info");
                break;
            case 502:
                ESP_LOGE(TAG, "Incompatible DCS version");
                break;
            case 503:
                ESP_LOGE(TAG, "Fileinfo: DCS is unavailable");
                duet_sbc_mode = false;
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
    sprintf(request_addr, "%s/rr_config", rep_addr_resolved);
    wifi_response_buff_t resp_buff_gui_task;
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = http_event_handle,
            .user_data = &resp_buff_gui_task,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        //ESP_LOGI(TAG, "Status = %d, content_length = %d", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));
    }
    switch (esp_http_client_get_status_code(client)) {
        case 200:
            // TODO process_reprap_config();
            break;
        case 401:
            //ESP_LOGI(TAG, "Authorising with Duet");
            wifi_duet_authorise(&resp_buff_gui_task, false);
            break;
        default:
            break;
    }
    esp_http_client_cleanup(client);
}


void reprap_wifi_download(wifi_response_buff_t *response_buffer, char *file) {
    //ESP_LOGI(TAG, "Downloading %s", file);
    char request_addr[MAX_REQ_ADDR_LENGTH];
    if (duet_sbc_mode) {
        sprintf(request_addr, "%s/machine/file/%s", rep_addr_resolved, file);
    } else {
        sprintf(request_addr, "%s/rr_download?name=%s", rep_addr_resolved, file);
    }
    esp_http_client_config_t config = {
            .url = request_addr,
            .timeout_ms = REQUEST_TIMEOUT_MS,
            .event_handler = http_event_handle,
            .user_data = response_buffer,
    };
    ESP_LOGI(TAG, "Downloading %s", request_addr);
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        //ESP_LOGI(TAG, "Status = %d, content_length = %d", esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));

        switch (esp_http_client_get_status_code(client)) {
            case 200:
                if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 100) == pdTRUE) {
                    process_reprap_settings(response_buffer->buffer);
                    xSemaphoreGive(xGuiSemaphore);
                }
                break;
            case 401:
                //ESP_LOGI(TAG, "Authorising with Duet");
                wifi_duet_authorise(response_buffer, false);
                break;
            case 500:
                ESP_LOGE(TAG, "Generic error downloading file");
                break;
            case 502:
                ESP_LOGE(TAG, "Incompatible DCS version");
                break;
            case 503:
                ESP_LOGE(TAG, "Wifi download: DCS is unavailable");
                duet_sbc_mode = false;
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
    if (rp_conn_stat == REPPANEL_WIFI_CONNECTED) {
        if (reprap_wifi_send_gcode(gcode_command)) {
            add_console_hist_entry(gcode_command, CONSOLE_TYPE_REPPANEL);
            update_entries_ui();
            return true;
        }
    } else if (rp_conn_stat == REPPANEL_UART_CONNECTED) {
        reprap_uart_send_gcode(gcode_command);
        add_console_hist_entry(gcode_command, CONSOLE_TYPE_REPPANEL);
        update_entries_ui();
        return true;
    }
    return false;
}

/**
 * Launches a new thread that requests macros. Updates Macros list in GUI on success. Non blocking call.
 * @param folder_path e.g.
 */
void request_macros_async(char *folder_path) {
    if (rp_conn_stat == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGW(TAG, "Requesting macros async not stable!");
        TaskHandle_t get_filelist_async_task_handle = NULL;
        xTaskCreate(reprap_wifi_get_filelist_task, "macros request task", 1024 * 5, folder_path,
                    tskIDLE_PRIORITY, &get_filelist_async_task_handle);
        configASSERT(get_filelist_async_task_handle);
    } else if (rp_conn_stat == REPPANEL_UART_CONNECTED) {
        request_macros(folder_path);
    }
}

void request_macros(char *folder_path) {
    if (rp_conn_stat == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting macros");
        strncpy(request_file_path, folder_path, sizeof(request_file_path));  // buffer path to request
        duet_request_macros = true; // Set flag - processed by status update task
    } else if (rp_conn_stat == REPPANEL_UART_CONNECTED) {
        strncpy(request_file_path, folder_path, sizeof(request_file_path));  // buffer path to request
        duet_request_macros = true;
    }
}

/**
 * Updates internal global variables with file info
 * @param file_name Path to file name on printer local storage. NULL in case you need file info of currently printed file
 */
void request_fileinfo(char *file_name) {
    if (rp_conn_stat == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting file info");
        wifi_response_buff_t resp_buff_gui_task;
        reprap_wifi_get_fileinfo(&resp_buff_gui_task, file_name);
    } else if (rp_conn_stat == REPPANEL_UART_CONNECTED) {
        uart_request_file_info = true;
        if (file_name != NULL)
            strncpy(request_file_path, file_name, sizeof(request_file_path));
        else
            strcpy(request_file_path, "");
    }
}

/**
 * Launches a new thread that requests job list. Updates Job list in GUI on success. Non blocking call.
 */
void request_jobs_async(char *folder_path) {
    if (rp_conn_stat == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGW(TAG, "Requesting jobs async not stable");
        TaskHandle_t get_filelist_async_task_handle = NULL;
        xTaskCreate(reprap_wifi_get_filelist_task, "jobs request task", 1024 * 5, folder_path,
                    tskIDLE_PRIORITY, &get_filelist_async_task_handle);
        configASSERT(get_filelist_async_task_handle);
    } else if (rp_conn_stat == REPPANEL_UART_CONNECTED) {
        request_jobs(folder_path);
    }
}

void request_jobs(char *folder_path) {
    if (rp_conn_stat == REPPANEL_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Requesting jobs");
        strncpy(request_file_path, folder_path, sizeof(request_file_path));  // buffer path to request
        duet_request_jobs = true; // Set flag - processed by status update task
    } else if (rp_conn_stat == REPPANEL_UART_CONNECTED) {
        strncpy(request_file_path, folder_path, sizeof(request_file_path));  // buffer path to request
        duet_request_jobs = true;   // set flag so task knows what to do in next iteration
    }
}

void request_rrf_status(uart_response_buff_t *receive_buff, wifi_response_buff_t *resp_buff, int type, char *key,
                        char *flags) {
    if (rp_conn_stat == REPPANEL_UART_CONNECTED) {
        reprap_uart_get_status(receive_buff, type, key, flags);
    } else if (rp_conn_stat == REPPANEL_WIFI_CONNECTED ||
               rp_conn_stat == REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED) {
        reprap_wifi_get_status(resp_buff, type, key, flags);
    }
}

void request_rrf3_extended_info(uart_response_buff_t *uart_receive_buff, wifi_response_buff_t *wifi_resp_buff) {
    if (reprap_model.reprap_seqs_changed.tools_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "tools", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.sensors_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "sensors", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.state_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "state", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.network_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "network", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.move_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "move", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.job_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "job", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.inputs_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "inputs", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.heat_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "heat", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.global_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "global", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.fans_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "fans", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.network_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "network", "d99vn");
    }
    if (reprap_model.reprap_seqs_changed.directories_changed) {
        request_rrf_status(uart_receive_buff, wifi_resp_buff, 2, "directories", "d99vn");
    }
}

bool update_printer_addr() {
    ESP_LOGI(TAG, "Updating printer address");
    if (ends_with(rep_addr, ".local")) {
        char tmp_addr[strlen(rep_addr)];
        strcpy(tmp_addr, rep_addr);
        tmp_addr[strlen(tmp_addr) - 6] = '\0';
        memmove(tmp_addr, tmp_addr + 7, strlen(tmp_addr)); // cut off http://
        ESP_LOGI(TAG, "Resolving %s", tmp_addr);
        char tmp_res[32];
        if (resolve_mdns_host(tmp_addr, tmp_res)) {
            strcpy(rep_addr_resolved, tmp_res);
            return true;
        }
        return false;
    }
    return true;    // no resolving required. User entered IP directly
}

/**
 * Called every 750ms
 * @param task
 */
void request_reprap_status_updates(void *params) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = (500 / portTICK_PERIOD_MS);
    xLastWakeTime = xTaskGetTickCount();
    int i = 0, b = 0;
    UBaseType_t uxHighWaterMark;
    uart_response_buff_t uart_receive_buff;
    wifi_response_buff_t resp_buff_status_update_task;
    while (strlen(rep_addr) < 1) {  // wait till request addr is set
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    strncpy(rep_addr_resolved, rep_addr, 256);
    bool init_printer_addr_updated = false;
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        if (rp_conn_stat == REPPANEL_UART_CONNECTED) {
            if (!got_duet_settings) {
                if (reprap_model.api_level < 1) {  // RRF2
                    reprap_uart_download(&uart_receive_buff, "0:/sys/dwc2settings.json");   // get dummy values
                } else {
                    reprap_uart_download(&uart_receive_buff, "0:/sys/dwc-settings.json");   // get dummy values
                    request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "boards", "d99vn");
                    request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "fans", "d99vn");
                    request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "heat", "d99vn");
                    request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "job", "d99vn");
                    request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "move", "d99vn");
                    request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "tools", "d99vn");
                }
            }
            if (!got_filaments) reprap_uart_get_filelist(&uart_receive_buff, "0:/filaments");
            if (uart_request_file_info) reprap_uart_get_file_info(&uart_receive_buff);
            if (duet_request_jobs) {
                reprap_uart_get_filelist(&uart_receive_buff, request_file_path);
                duet_request_jobs = false;
            }
            if (duet_request_macros) {
                reprap_uart_get_filelist(&uart_receive_buff, request_file_path);
                duet_request_macros = false;
            }
            if (!got_extended_status) request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 3, "", "");
            if (!job_running)
                request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "", "d99fn");
            else
                request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 4, "", "d99fn");
            if (reprap_model.api_level >= 1) {
                request_rrf3_extended_info(&uart_receive_buff, &resp_buff_status_update_task);
            }
            if (i == 20) {
                request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 3, "", "d99fn");
                i = 0;
            } else { i++; }
        } else if (rp_conn_stat == REPPANEL_WIFI_CONNECTED ||
                   rp_conn_stat == REPPANEL_WIFI_CONNECTED_DUET_DISCONNECTED) {
            if (init_printer_addr_updated) {
                if (!got_duet_settings) {
                    if (reprap_model.api_level < 1) {  // RRF2
                        reprap_wifi_download(&resp_buff_status_update_task, "0%3A%2Fsys%2Fdwc2settings.json");
                    } else {
                        reprap_wifi_download(&resp_buff_status_update_task, "0%3A%2Fsys%2Fdwc-settings.json");
                        request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "boards", "d99vn");
                        request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "fans", "d99vn");
                        request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "heat", "d99vn");
                        request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "job", "d99vn");
                        request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "move", "d99vn");
                        request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "tools", "d99vn");
                    }
                }
                if (!got_filaments) reprap_wifi_get_filelist(&resp_buff_status_update_task, "0:/filaments&first=0");
                if (reprap_model.api_level < 1) {  // RRF2
                    if (!got_extended_status)
                        request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 2, "", "d99fn");
                }
                // for synchron request of jobs
                if (duet_request_jobs) {
                    reprap_wifi_get_filelist(&resp_buff_status_update_task, request_file_path);
                    duet_request_jobs = false;
                }
                // for synchron request of macros
                if (duet_request_macros) {
                    reprap_wifi_get_filelist(&resp_buff_status_update_task, request_file_path);
                    duet_request_macros = false;
                }
                if (!job_running)
                    request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 0, "", "d99fn");
                else
                    request_rrf_status(&uart_receive_buff, &resp_buff_status_update_task, 3, "", "d99fn");
//                if (reprap_model.reprap_seqs_changed.reply_changed) {
//                    reprap_wifi_get_rreply(&resp_buff_status_update_task);
//                }
                if (reprap_model.api_level >= 1) {
                    request_rrf3_extended_info(&uart_receive_buff, &resp_buff_status_update_task);
                }

                if (i == 20) {
                    // Check if we got a UART connection
                    if (reppanel_is_uart_connected()) {
                        rp_conn_stat = REPPANEL_UART_CONNECTED;
                        memset(&resp_buff_status_update_task, 0, JSON_BUFF_SIZE);
                        if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
                            update_rep_panel_conn_status();
                            xSemaphoreGive(xGuiSemaphore);
                        }
                    }
                    i = 0;
                } else { i++; }
                if (b == 100) {
                    update_printer_addr();  // in case address has changed
                    b = 0;
                } else { b++; }
            } else {
                init_printer_addr_updated = update_printer_addr();  // initial resolving
            }
        } else {
            if (reppanel_is_uart_connected()) {
                rp_conn_stat = REPPANEL_UART_CONNECTED;
                memset(&resp_buff_status_update_task, 0, JSON_BUFF_SIZE);
                if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
                    update_rep_panel_conn_status();
                    xSemaphoreGive(xGuiSemaphore);
                }
            }
        }
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGD(TAG, "%i free bytes", uxHighWaterMark * 4);
    }
    vTaskDelete(NULL);
}
