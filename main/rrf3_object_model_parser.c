//
// Copyright (c) 2021 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <cJSON.h>
#include "rrf3_object_model_parser.h"
#include "reppanel.h"
#include "rrf_objects.h"

void reppanel_parse_rr_connect(cJSON *connect_result, reprap_model_t *_reprap_model) {
    cJSON *api_level = cJSON_GetObjectItemCaseSensitive(connect_result, "apiLevel");
    if (api_level)
        _reprap_model->api_level = api_level->valueint;
    // TODO: Implement session key support - Normal ESP32 Modules will not have enought RAM to receive the entire
    //  Object Model JSON at once. No use for now to support Duet3 SBC
//    cJSON *sessionKey = cJSON_GetObjectItemCaseSensitive(connect_result, "sessionKey");
//    if (sessionKey)
//        _reprap_model->session_key = sessionKey->valueint;
}

void reppanel_parse_rrf_boards(cJSON *boards_result, cJSON *flags, reprap_model_t *_reprap_model) {
    cJSON *board_temps = cJSON_GetArrayItem(boards_result, 0);
    if (board_temps) {
        cJSON *mcu_temp = cJSON_GetObjectItemCaseSensitive(board_temps, "mcuTemp");
        if (mcu_temp) {
            cJSON *current_mcu_temp = cJSON_GetObjectItemCaseSensitive(mcu_temp, "current");
            if (current_mcu_temp) reprap_mcu_temp = current_mcu_temp->valuedouble;
        }
    }
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.boards_changed = 0; }
}

void reppanel_parse_rrf_fans(cJSON *fans_result, cJSON *flags, reprap_model_t *_reprap_model) {
    if (cJSON_GetArraySize(fans_result) > 0) {
        cJSON *fan_zero = cJSON_GetArrayItem(fans_result, reprap_tools[0].fans);
        if (fan_zero) {
            cJSON *val = cJSON_GetObjectItemCaseSensitive(fan_zero, "actualValue");
            if (val) { reprap_params.fan = (int16_t) (val->valuedouble * 100); }
        }
    }
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.fans_changed = 0; }
}

void reppanel_parse_rrf_heaters(cJSON *heat_result, int *_heater_states, cJSON *flags, reprap_model_t *_reprap_model) {
    cJSON *bedHeaters = cJSON_GetObjectItemCaseSensitive(heat_result, "bedHeaters");
    if (bedHeaters)
        reprap_bed.heater_indx = cJSON_GetArrayItem(bedHeaters, 0)->valueint;  // only support one heater per bed
    cJSON *heaters = cJSON_GetObjectItem(heat_result, "heaters");
    reprap_model.num_heaters = cJSON_GetArraySize(heaters);
    if (reprap_model.num_heaters == 0)
        return;
    cJSON *heater = cJSON_GetArrayItem(heaters, reprap_bed.heater_indx);
    reprap_bed.active_temp = cJSON_GetObjectItemCaseSensitive(heater, "active")->valuedouble;
    reprap_bed.standby_temp = cJSON_GetObjectItemCaseSensitive(heater, "standby")->valuedouble;
    if (reprap_bed.temp_hist_curr_pos < (NUM_TEMPS_BUFF - 1)) {
        reprap_bed.temp_hist_curr_pos++;
    } else {
        reprap_bed.temp_hist_curr_pos = 0;
    }
    reprap_bed.temp_buff[reprap_bed.temp_hist_curr_pos] = cJSON_GetObjectItemCaseSensitive(heater,
                                                                                           "current")->valuedouble;
    cJSON *heater_state = cJSON_GetObjectItemCaseSensitive(heater, "state");
    if (heater_state->valuestring[0] == 'o') {
        _heater_states[0] = HEATER_OFF; // bed heater is always on index 0
    } else if (heater_state->valuestring[0] == 'a') {
        _heater_states[0] = HEATER_ACTIVE; // bed heater is always on index 0
    } else if (heater_state->valuestring[0] == 's') {
        _heater_states[0] = HEATER_STDBY; // bed heater is always on index 0
    } else {
        _heater_states[0] = HEATER_FAULT; // bed heater is always on index 0
    }
    // Tool heaters
    for (int i = 0; i < reprap_model.num_tools; i++) {
        if (reprap_tools[i].temp_hist_curr_pos < (NUM_TEMPS_BUFF - 1)) {
            reprap_tools[i].temp_hist_curr_pos++;
        } else {
            reprap_tools[i].temp_hist_curr_pos = 0;
        }
        heater = cJSON_GetArrayItem(heaters, reprap_tools[i].heater_indx);
        reprap_tools[i].temp_buff[reprap_tools[i].temp_hist_curr_pos] = cJSON_GetObjectItemCaseSensitive(heater,
                                                                                                         "current")->valuedouble;

        heater_state = cJSON_GetObjectItemCaseSensitive(heater, "state");
        if (heater_state->valuestring[0] == 'o') {
            _heater_states[i + 1] = HEATER_OFF; // bed heater is always on index 0
        } else if (heater_state->valuestring[0] == 'a') {
            _heater_states[i + 1] = HEATER_ACTIVE; // bed heater is always on index 0
        } else if (heater_state->valuestring[0] == 's') {
            _heater_states[i + 1] = HEATER_STDBY; // bed heater is always on index 0
        } else {
            _heater_states[i + 1] = HEATER_FAULT; // bed heater is always on index 0
        }
    }
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.heat_changed = 0; }
}

void reppanel_parse_rrf_tools(cJSON *tools_result, int *_heater_states, cJSON *flags, reprap_model_t *_reprap_model) {
    if (!cJSON_IsArray(tools_result))
        return;
    reprap_model.num_tools = cJSON_GetArraySize(tools_result);
    // Get tool temperatures
    for (int i = 0; i < reprap_model.num_tools; i++) {
        cJSON *tool = cJSON_GetArrayItem(tools_result, i);
        cJSON *heaters = cJSON_GetObjectItemCaseSensitive(tool, "heaters");
        if (heaters && cJSON_GetArraySize(heaters) > 0) {
            cJSON *v = cJSON_GetArrayItem(heaters, 0);
            reprap_tools[i].heater_indx = v->valueint;
        }

        cJSON *val = cJSON_GetObjectItemCaseSensitive(tool, "name");
        if (val && cJSON_IsString(val) && (val->valuestring != NULL)) {
            strncpy(reprap_tools[i].name, val->valuestring, MAX_TOOL_NAME_LEN - 1);
        }
        val = cJSON_GetObjectItemCaseSensitive(tool, "number");
        if (val && cJSON_GetArraySize(val) > 0) reprap_tools[i].number = val->valueint;
        cJSON *active_temp_array = cJSON_GetObjectItemCaseSensitive(tool, "active");
        cJSON *standby_temp_array = cJSON_GetObjectItemCaseSensitive(tool, "standby");
        if (cJSON_GetArraySize(active_temp_array) > 0)
            reprap_tools[i].active_temp = cJSON_GetArrayItem(active_temp_array, 0)->valuedouble;
        if (cJSON_GetArraySize(standby_temp_array) > 0)
            reprap_tools[i].standby_temp = cJSON_GetArrayItem(standby_temp_array, 0)->valuedouble;
        val = cJSON_GetObjectItemCaseSensitive(tool, "fans");
        if (val) {
            reprap_tools[i].fans = cJSON_GetArrayItem(val, 0)->valueint;
        }
    }
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.tools_changed = 0; }
}

/**
 * Read file info from json to reprap model
 * @param root pre-parsed cJSON pointer
 * @param _reprap_model Pointer to the reprap model to store the data
 */
void reppanel_parse_file_info(cJSON *root, reprap_model_t *_reprap_model) {
    cJSON *val = cJSON_GetObjectItemCaseSensitive(root, "size");
    if (val && cJSON_IsNumber(val)) _reprap_model->reprap_job.file.size = val->valueint;
    val = cJSON_GetObjectItemCaseSensitive(root, "numLayers");
    if (val && cJSON_IsNumber(val)) _reprap_model->reprap_job.file.numLayers = val->valueint;
    val = cJSON_GetObjectItemCaseSensitive(root, "height");
    if (val && cJSON_IsNumber(val)) _reprap_model->reprap_job.file.height = (float) val->valuedouble;
    val = cJSON_GetObjectItemCaseSensitive(root, "firstLayerHeight");
    if (val && cJSON_IsNumber(val)) reprap_job_first_layer_height = val->valuedouble;
    val = cJSON_GetObjectItemCaseSensitive(root, "layerHeight");
    if (val && cJSON_IsNumber(val)) reprap_job_layer_height = val->valuedouble;
    val = cJSON_GetObjectItemCaseSensitive(root, "fileName");
    if (cJSON_IsString(val) && (val->valuestring != NULL))
        strncpy(_reprap_model->reprap_job.file.fileName, &val->valuestring[9], MAX_LEN_FILENAME - 1);
    val = cJSON_GetObjectItemCaseSensitive(root, "simulatedTime");
    if (val && cJSON_IsNumber(val)) {
        _reprap_model->reprap_job.file.simulatedTime = val->valueint;
    } else {
        _reprap_model->reprap_job.file.simulatedTime = 0;
    }
    val = cJSON_GetObjectItemCaseSensitive(root, "printTime");
    if (val && cJSON_IsNumber(val)) { _reprap_model->reprap_job.file.printTime = val->valueint; }

    val = cJSON_GetObjectItemCaseSensitive(root, "filament");
    cJSON *filament_usage = NULL;
    reprap_model.reprap_job.file.overall_filament_usage = 0;
    cJSON_ArrayForEach(filament_usage, val) {
        reprap_model.reprap_job.file.overall_filament_usage += filament_usage->valuedouble;
    }
    cJSON *job_thumbnails = cJSON_GetObjectItem(root, "thumbnails");
    if (job_thumbnails && cJSON_IsArray(job_thumbnails)) {
        cJSON *thumbnail_obj = NULL;
        cJSON_ArrayForEach(thumbnail_obj, job_thumbnails) {
            cJSON *var = cJSON_GetObjectItemCaseSensitive(thumbnail_obj, "fmt");
            if (var && cJSON_IsString(var)) {
                // TODO analyze format
            }
            var = cJSON_GetObjectItemCaseSensitive(thumbnail_obj, "w");
            if (var && cJSON_IsNumber(var)) {
                // TODO analyze width
            }
            var = cJSON_GetObjectItemCaseSensitive(thumbnail_obj, "h");
            if (var && cJSON_IsNumber(var)) {
                // TODO analyze height
            }
        }
    }
}

/**
 * Parse RRF object model job information
 * @param job_result The json result of a job query. The "result" object (status request) or the "job" object
 * (job request)
 * @return If a job is currently running
 */
void reppanel_parse_rrf_job(cJSON *job_result, cJSON *flags, reprap_model_t *_reprap_model) {
    _reprap_model->reprap_job.duration = cJSON_GetObjectItemCaseSensitive(job_result, "duration")->valueint;
    _reprap_model->reprap_job.layer = cJSON_GetObjectItemCaseSensitive(job_result, "layer")->valueint;
    _reprap_model->reprap_job.filePosition = cJSON_GetObjectItemCaseSensitive(job_result, "filePosition")->valueint;
    _reprap_model->reprap_job.rawExtrusion = cJSON_GetObjectItemCaseSensitive(job_result, "rawExtrusion")->valuedouble;
    cJSON *times_left = cJSON_GetObjectItem(job_result, "timesLeft");
    if (times_left) {
        cJSON *sim_time = cJSON_GetObjectItemCaseSensitive(times_left, "simulation");
        // Not so beautiful code I know
        if (sim_time && cJSON_IsNumber(sim_time)) {
            _reprap_model->reprap_job.timesLeft.simulation = sim_time->valueint;
        } else {
            cJSON *time_slicer = cJSON_GetObjectItemCaseSensitive(times_left, "slicer");
            if (time_slicer && cJSON_IsNumber(time_slicer)) {
                _reprap_model->reprap_job.timesLeft.slicer = time_slicer->valueint;
            } else {
                cJSON *time_file = cJSON_GetObjectItemCaseSensitive(times_left, "file");
                if (time_file && cJSON_IsNumber(time_file)) {
                    _reprap_model->reprap_job.timesLeft.file = time_file->valueint;
                }
            }
        }
    }
    cJSON *file = cJSON_GetObjectItemCaseSensitive(job_result, "file");
    if (file) {
        reppanel_parse_file_info(file, _reprap_model);
    }
    if (_reprap_model->reprap_job.file.overall_filament_usage > 0) {
        reprap_job_percent = (float) (
                (_reprap_model->reprap_job.rawExtrusion / _reprap_model->reprap_job.file.overall_filament_usage) * 100);
    } else {
        reprap_job_percent =
                ((float) _reprap_model->reprap_job.filePosition / (float) _reprap_model->reprap_job.file.size) * 100.0f;
    }
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.job_changed = 0; }
}

void reppanel_parse_rrf_move(cJSON *move_result, cJSON *flags, reprap_model_t *_reprap_model) {
    cJSON *axes = cJSON_GetObjectItemCaseSensitive(move_result, "axes");
    const cJSON *axis = NULL;
    int i = 0;
    cJSON_ArrayForEach(axis, axes) {
        if (i < REPPANEL_RRF_MAX_AXES) {
            cJSON *queried_obj = cJSON_GetObjectItemCaseSensitive(axis, "machinePosition");
            if (queried_obj && cJSON_IsNumber(queried_obj)) reprap_axes.axes[i] = queried_obj->valuedouble;
            queried_obj = cJSON_GetObjectItem(axis, "homed");
            if (queried_obj) reprap_axes.homed[i] = cJSON_IsTrue(queried_obj);
            queried_obj = cJSON_GetObjectItemCaseSensitive(axis, "letter");
            if (queried_obj && cJSON_IsString(queried_obj) && (queried_obj->valuestring != NULL))
                reprap_axes.letter[i] = queried_obj->valuestring[0];
            queried_obj = cJSON_GetObjectItemCaseSensitive(axis, "min");
            if (queried_obj && cJSON_IsNumber(queried_obj)) reprap_axes.min[i] = queried_obj->valuedouble;
            queried_obj = cJSON_GetObjectItemCaseSensitive(axis, "max");
            if (queried_obj && cJSON_IsNumber(queried_obj)) reprap_axes.max[i] = queried_obj->valuedouble;
            queried_obj = cJSON_GetObjectItemCaseSensitive(axis, "babystep");
            if (queried_obj && cJSON_IsNumber(queried_obj)) reprap_axes.babystep[i] = queried_obj->valuedouble;
        }
        i++;
    }
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.move_changed = 0; }
}

void reppanel_parse_rrf_state(cJSON *state_result, cJSON *flags, reprap_model_t *_reprap_model) {
    cJSON *val = cJSON_GetObjectItemCaseSensitive(state_result, "status");
    if (val && cJSON_IsString(val) && (val->valuestring != NULL))
        strncpy(_reprap_model->reprap_state.status, val->valuestring, REPRAP_MAX_STATUS_LEN - 1);
//    val = cJSON_GetObjectItemCaseSensitive(state_result, "displayMessage");
//    if (val && cJSON_IsString(val) && (val->valuestring != NULL)) {
//        strncpy(_reprap_model->reprap_state.msg_box_msg, val->valuestring, REPRAP_MAX_DISPLAY_MSG_LEN - 1);
//    }
    val = cJSON_GetObjectItemCaseSensitive(state_result, "messageBox");
    if (val && !cJSON_IsNull(val)) {
        _reprap_model->reprap_state.new_msg = true;
        cJSON *new_val = cJSON_GetObjectItemCaseSensitive(val, "title");
        strncpy(_reprap_model->reprap_state.msg_box_title, new_val->valuestring, REPRAP_MAX_LEN_MSG_TITLE - 1);
        new_val = cJSON_GetObjectItemCaseSensitive(val, "message");
        strncpy(_reprap_model->reprap_state.msg_box_msg, new_val->valuestring, REPRAP_MAX_DISPLAY_MSG_LEN - 1);
        new_val = cJSON_GetObjectItemCaseSensitive(val, "axisControls");
        _reprap_model->reprap_state.show_axis_controls = new_val->valueint;
        new_val = cJSON_GetObjectItemCaseSensitive(val, "mode");
        _reprap_model->reprap_state.mode = new_val->valueint;
        new_val = cJSON_GetObjectItemCaseSensitive(val, "timeout");
        _reprap_model->reprap_state.timeout = new_val->valueint;
    }
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.state_changed = 0; }
}

void reppanel_parse_rrf_network(cJSON *network_result, cJSON *flags, reprap_model_t *_reprap_model) {
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.network_changed = 0; }
}

void reppanel_parse_rrf_sensors(cJSON *sensors_result, cJSON *flags, reprap_model_t *_reprap_model) {
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.sensors_changed = 0; }
}

void reppanel_parse_rrf_inputs(cJSON *input_result, cJSON *flags, reprap_model_t *_reprap_model) {
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.inputs_changed = 0; }
}

void reppanel_parse_rrf_global(cJSON *global_result, cJSON *flags, reprap_model_t *_reprap_model) {
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.global_changed = 0; }
}

void reppanel_parse_rrf_directories(cJSON *directories_result, cJSON *flags, reprap_model_t *_reprap_model) {
    if (strcmp(flags->valuestring, "d99vn") == 0) { _reprap_model->reprap_seqs_changed.directories_changed = 0; }
}

/**
 * Check if the sequence numbers changed and update them. If they changed you need to request the new information
 * @param seqs_result
 * @return true if a new msg. can be displayed
 */
void reppanel_parse_rrf_seqs(cJSON *seqs_result, reprap_model_t *_reprap_model) {
    cJSON *seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "boards");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.boards != seq->valueint) {
            _reprap_model->reprap_seqs.boards = seq->valueint;
            _reprap_model->reprap_seqs_changed.boards_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "directories");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.directories != seq->valueint) {
            _reprap_model->reprap_seqs.directories = seq->valueint;
            _reprap_model->reprap_seqs_changed.directories_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "fans");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.fans != seq->valueint) {
            _reprap_model->reprap_seqs.fans = seq->valueint;
            _reprap_model->reprap_seqs_changed.fans_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "global");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.global != seq->valueint) {
            _reprap_model->reprap_seqs.global = seq->valueint;
            _reprap_model->reprap_seqs_changed.global_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "heat");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.heat != seq->valueint) {
            _reprap_model->reprap_seqs.heat = seq->valueint;
            _reprap_model->reprap_seqs_changed.heat_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "inputs");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.inputs != seq->valueint) {
            _reprap_model->reprap_seqs.inputs = seq->valueint;
            _reprap_model->reprap_seqs_changed.inputs_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "job");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.job != seq->valueint) {
            _reprap_model->reprap_seqs.job = seq->valueint;
            _reprap_model->reprap_seqs_changed.job_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "move");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.move != seq->valueint) {
            _reprap_model->reprap_seqs.move = seq->valueint;
            _reprap_model->reprap_seqs_changed.move_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "network");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.network != seq->valueint) {
            _reprap_model->reprap_seqs.network = seq->valueint;
            _reprap_model->reprap_seqs_changed.network_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "reply");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.reply != seq->valueint) {
            _reprap_model->reprap_seqs.reply = seq->valueint;
            _reprap_model->reprap_seqs_changed.reply_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "sensors");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.sensors != seq->valueint) {
            _reprap_model->reprap_seqs.sensors = seq->valueint;
            _reprap_model->reprap_seqs_changed.sensors_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "state");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.state != seq->valueint) {
            _reprap_model->reprap_seqs.state = seq->valueint;
            _reprap_model->reprap_seqs_changed.state_changed = 1;
        }
    }
    seq = cJSON_GetObjectItemCaseSensitive(seqs_result, "tools");
    if (seq && cJSON_IsNumber(seq)) {
        if (_reprap_model->reprap_seqs.tools != seq->valueint) {
            _reprap_model->reprap_seqs.tools = seq->valueint;
            _reprap_model->reprap_seqs_changed.tools_changed = 1;
        }
    }
}

void reppanel_parse_rr_fileinfo(char *json_response, reprap_model_t *_reprap_model, int buff_length) {
    cJSON *root = cJSON_ParseWithLength(json_response, buff_length);
    if (root == NULL) {
        cJSON_Delete(root);
        return;
    }
    cJSON *err_resp = cJSON_GetObjectItem(root, "err");
    if (err_resp && err_resp->valueint != 0) {     // maybe no active print
        cJSON_Delete(root);
        return;
    }
    reppanel_parse_file_info(root, _reprap_model);
    cJSON_Delete(root);
}