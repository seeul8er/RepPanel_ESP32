//
// Copyright (c) 2021 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include "esp32_settings.h"

#ifndef REPPANEL_ESP32_RRF3_OBJECT_MODEL_PARSER_H
#define REPPANEL_ESP32_RRF3_OBJECT_MODEL_PARSER_H

#endif //REPPANEL_ESP32_RRF3_OBJECT_MODEL_PARSER_H

void reppanel_parse_rrf_boards(cJSON *result);
void reppanel_parse_rrf_fans(cJSON *result);
void reppanel_parse_rrf_heaters(cJSON *result, int _heater_states[MAX_NUM_TOOLS]);
void reppanel_parse_rrf_job(cJSON *result);
