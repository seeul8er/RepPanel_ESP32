//
// Copyright (c) 2021 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <cJSON.h>
#include "rrf3_object_model_parser.h"
#include "reppanel.h"

void reppanel_parse_rrf_boards(cJSON *result) {
    cJSON *boards = cJSON_GetObjectItemCaseSensitive(result, "boards");
    if (!boards) return;
    cJSON *board_temps = cJSON_GetArrayItem(boards, 0);
    if (board_temps) {
        cJSON *mcu_temp = cJSON_GetObjectItemCaseSensitive(board_temps, "mcuTemp");
        reprap_mcu_temp = cJSON_GetObjectItemCaseSensitive(mcu_temp, "current")->valuedouble;
    }
}

void reppanel_parse_rrf_fans(cJSON *result) {
    cJSON *fans = cJSON_GetObjectItemCaseSensitive(result, "fans");
    if (!fans) return;
    cJSON *fan_zero = cJSON_GetArrayItem(fans, 0);
    if (fan_zero) {
        reprap_params.fan = (int) cJSON_GetObjectItemCaseSensitive(fan_zero, "actualValue")->valuedouble * 100;
    }
}

void reppanel_parse_rrf_heaters(cJSON *result, int *_heater_states) {
    cJSON *heat = cJSON_GetObjectItem(result, "heat");
    if (!heat) return;
    cJSON *bedHeaters = cJSON_GetObjectItemCaseSensitive(heat, "bedHeaters");
    if (bedHeaters)
        reprap_bed.heater_indx = cJSON_GetArrayItem(bedHeaters, 0)->valueint;
    cJSON *heaters = cJSON_GetObjectItem(heat, "heaters");
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

void reppanel_parse_rrf_job(cJSON *result) {
    cJSON *job = cJSON_GetObjectItem(result, "job");
    if (!job) return;
    reprap_job_duration = cJSON_GetObjectItemCaseSensitive(job, "duration")->valuedouble;
    reprap_job_curr_layer = cJSON_GetObjectItemCaseSensitive(job, "layer")->valueint;
    cJSON *times_left = cJSON_GetObjectItem(job, "timesLeft");
    if (times_left) {
        cJSON *sim_time = cJSON_GetObjectItem(times_left, "simulation");
        // Not so beautiful code I know
        if (sim_time && cJSON_IsNumber(sim_time)) { // remove -> simulatedTime
            reprap_job_time_sim = sim_time->valueint;
            reprap_job_percent = (reprap_job_time_sim/reprap_job_duration) * 100;
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
    cJSON *file = cJSON_GetObjectItem(job, "file");
    if (file) {
        reprap_job_height =
    }
}