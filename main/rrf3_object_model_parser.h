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
void reppanel_parse_rrf_boards(cJSON *boards_result);
void reppanel_parse_rrf_fans(cJSON *fans_result);
void reppanel_parse_rrf_heaters(cJSON *heaters_result, int _heater_states[MAX_NUM_TOOLS]);
void reppanel_parse_rrf_tools(cJSON *tools_result, int _heater_states[MAX_NUM_TOOLS]);
void reppanel_parse_rrf_job(cJSON *job_result, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_move(cJSON *move_result);
void reppanel_parse_rrf_state(cJSON *state_result, reprap_model_t *_reprap_model);
void reppanel_parse_rrf_seqs(cJSON *seqs_result, reprap_model_t *_reprap_model);
