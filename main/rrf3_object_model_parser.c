//
// Copyright (c) 2021 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <cJSON.h>
#include "rrf3_object_model_parser.h"
#include "reppanel.h"
#include "rrf_objects.h"

void reprap_calc_job_percent() {
    reprap_job_percent = (reprap_job_time_sim/reprap_job_duration) * 100;
}

void reppanel_parse_rr_connect(cJSON *connect_result) {
        reprap_model.api_level = cJSON_GetObjectItemCaseSensitive(connect_result, "apiLevel")->valueint;
}

void reppanel_parse_rrf_boards(cJSON *boards_result) {
    cJSON *board_temps = cJSON_GetArrayItem(boards_result, 0);
    if (board_temps) {
        cJSON *mcu_temp = cJSON_GetObjectItemCaseSensitive(board_temps, "mcuTemp");
        reprap_mcu_temp = cJSON_GetObjectItemCaseSensitive(mcu_temp, "current")->valuedouble;
    }
}

void reppanel_parse_rrf_fans(cJSON *fans_result) {
    cJSON *fan_zero = cJSON_GetArrayItem(fans_result, 0);
    if (fan_zero) {
        reprap_params.fan = (int) cJSON_GetObjectItemCaseSensitive(fan_zero, "actualValue")->valuedouble * 100;
    }
}

void reppanel_parse_rrf_heaters(cJSON *heat_result, int *_heater_states) {
    cJSON *bedHeaters = cJSON_GetObjectItemCaseSensitive(heat_result, "bedHeaters");
    if (bedHeaters)
        reprap_bed.heater_indx = cJSON_GetArrayItem(bedHeaters, 0)->valueint;
    cJSON *heaters = cJSON_GetObjectItem(heat_result, "heaters");
    cJSON *bed_heater = cJSON_GetArrayItem(heaters, reprap_bed.heater_indx);
    reprap_bed.active_temp = cJSON_GetObjectItemCaseSensitive(bed_heater, "active")->valuedouble;
    reprap_bed.standby_temp = cJSON_GetObjectItemCaseSensitive(bed_heater, "standby")->valuedouble;
    if (reprap_bed.temp_hist_curr_pos < (NUM_TEMPS_BUFF - 1)) {
        reprap_bed.temp_hist_curr_pos++;
    } else {
        reprap_bed.temp_hist_curr_pos = 0;
    }
    reprap_bed.temp_buff[reprap_bed.temp_hist_curr_pos] = cJSON_GetObjectItemCaseSensitive(bed_heater, "current")->valuedouble;
    cJSON *bed_heater_state = cJSON_GetObjectItemCaseSensitive(bed_heater, "state");
    if (bed_heater_state->string[0] == 'o') {
        _heater_states[0] = HEATER_OFF; // bed heater is always on index 0
    } else if (bed_heater_state->string[0] == 'a') {
        _heater_states[0] = HEATER_ACTIVE; // bed heater is always on index 0
    } else if (bed_heater_state->string[0] == 's') {
        _heater_states[0] = HEATER_STDBY; // bed heater is always on index 0
    } else {
        _heater_states[0] = HEATER_FAULT; // bed heater is always on index 0
    }
}

/**
 * Parse RRF object model job information
 * @param job_result The json result of a job query. The "result" object (status request) or the "job" object
 * (job request)
 */
void reppanel_parse_rrf_job(cJSON *job_result) {
    reprap_job_duration = cJSON_GetObjectItemCaseSensitive(job_result, "duration")->valuedouble;
    reprap_job_curr_layer = cJSON_GetObjectItemCaseSensitive(job_result, "layer")->valueint;
    cJSON *times_left = cJSON_GetObjectItem(job_result, "timesLeft");
    if (times_left) {
        cJSON *sim_time = cJSON_GetObjectItemCaseSensitive(times_left, "simulation");
        // Not so beautiful code I know
        if (sim_time && cJSON_IsNumber(sim_time)) { // remove -> simulatedTime
            reprap_job_time_sim = sim_time->valueint;
            reprap_calc_job_percent();
        } else {
            cJSON *time_slicer = cJSON_GetObjectItem(times_left, "slicer");
            if (time_slicer && cJSON_IsNumber(time_slicer)) {
                reprap_job_time_slicer = time_slicer->valueint;
                reprap_job_percent = (reprap_job_time_slicer/reprap_job_duration) * 100;
            } else {
                cJSON *time_file = cJSON_GetObjectItem(times_left, "file");
                if (time_file && cJSON_IsNumber(time_file)) {
                    reprap_job_time_file = time_file->valueint;
                    reprap_job_percent = (reprap_job_time_file/reprap_job_duration) * 100;
                }
            }
        }
    }
    cJSON *file = cJSON_GetObjectItem(job_result, "file");
    if (file) {
        cJSON *numLayers = cJSON_GetObjectItem(file, "numLayers");
        if (numLayers) reprap_job_numlayers = numLayers->valueint;
        cJSON *height = cJSON_GetObjectItem(file, "height");
        if (height) reprap_job_height = height->valuedouble;
        cJSON *firstLayerHeight = cJSON_GetObjectItem(file, "firstLayerHeight");
        if (firstLayerHeight) reprap_job_first_layer_height = firstLayerHeight->valuedouble;
        cJSON *layerHeight = cJSON_GetObjectItem(file, "layerHeight");
        if (layerHeight) reprap_job_layer_height = layerHeight->valuedouble;
        cJSON *fileName = cJSON_GetObjectItem(file, "fileName");
        if (fileName) strncpy(reprap_job_name, fileName->string, MAX_LEN_FILENAME);
        cJSON *simulatedTime = cJSON_GetObjectItemCaseSensitive(file, "simulatedTime");
        if (simulatedTime && cJSON_IsNumber(simulatedTime)) {
            reprap_job_time_sim = simulatedTime->valueint;
            reprap_calc_job_percent();
        }
        cJSON *duration = cJSON_GetObjectItem(file, "duration");
        if (duration) reprap_job_duration = duration->valuedouble;
        cJSON *layer = cJSON_GetObjectItem(file, "layer");
        if (layer) reprap_job_curr_layer = layer->valueint;
    }
}

void reppanel_parse_rrf_move(cJSON *move_result) {
    cJSON *axes = cJSON_GetObjectItemCaseSensitive(move_result, "axes");
    const cJSON *axis = NULL;
    int i = 0;
    cJSON_ArrayForEach(axis, axes) {
        if (i < REPPANEL_RRF_MAX_AXES) {
            cJSON *queried_obj = cJSON_GetObjectItem(axis, "machinePosition");
            if (queried_obj) reprap_axes.axes[i] = queried_obj->valuedouble;
            queried_obj = cJSON_GetObjectItem(axis, "homed");
            if (queried_obj) reprap_axes.homed[i] = cJSON_IsTrue(queried_obj);
            queried_obj = cJSON_GetObjectItem(axis, "letter");
            if (queried_obj) reprap_axes.letter[i] = queried_obj->valuestring[0];
            queried_obj = cJSON_GetObjectItem(axis, "min");
            if (queried_obj) reprap_axes.min[i] = queried_obj->valuedouble;
            queried_obj = cJSON_GetObjectItem(axis, "max");
            if (queried_obj) reprap_axes.max[i] = queried_obj->valuedouble;
            queried_obj = cJSON_GetObjectItem(axis, "babystep");
            if (queried_obj) reprap_axes.babystep[i] = queried_obj->valuedouble;
        }
        i++;
    }
}

void reppanel_parse_rrf_state(cJSON *state_result) {
    cJSON *status = cJSON_GetObjectItemCaseSensitive(state_result, "status");
    if (status) strncpy(reppanel_status, cJSON_GetStringValue(status), MAX_REPRAP_STATUS_LEN);
}

/**
 * Check if a new reply is available
 * @param seqs_result
 * @return true if a new msg. can be displayed
 */
bool reppanel_parse_rrf_seqs(cJSON *seqs_result) {
    cJSON *reply = cJSON_GetObjectItemCaseSensitive(seqs_result, "reply");
    if (reply && cJSON_IsNumber(reply)) {
        if (reprap_model.reprap_seqs.reply != reply->valueint) {
            return true;
        }
        reprap_model.reprap_seqs.reply = reply->valueint;
    }
    return false;
}