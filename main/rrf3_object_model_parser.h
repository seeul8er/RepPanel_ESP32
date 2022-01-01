//
// Copyright (c) 2021 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <stdbool.h>
#include "esp32_settings.h"
#include "rrf_objects.h"

#ifndef REPPANEL_ESP32_RRF3_OBJECT_MODEL_PARSER_H
#define REPPANEL_ESP32_RRF3_OBJECT_MODEL_PARSER_H

#endif //REPPANEL_ESP32_RRF3_OBJECT_MODEL_PARSER_H

void reppanel_parse_rr_connect(cJSON *connect_result, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_boards(cJSON *boards_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_fans(cJSON *fans_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_heaters(cJSON *heaters_result, int _heater_states[MAX_NUM_TOOLS], cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_tools(cJSON *tools_result, int _heater_states[MAX_NUM_TOOLS], cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_job(cJSON *job_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_move(cJSON *move_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_state(cJSON *state_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_network(cJSON *network_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_sensors(cJSON *sensors_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_inputs(cJSON *input_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_global(cJSON *global_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_directories(cJSON *directories_result, cJSON *flags, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_seqs(cJSON *seqs_result, reprap_model_t *_reprap_model);
