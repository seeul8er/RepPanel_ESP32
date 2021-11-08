//
// Copyright (c) 2021 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef REPPANEL_ESP32_RRF_OBJECTS_H
#define REPPANEL_ESP32_RRF_OBJECTS_H

typedef struct {
    int api_level;
} reprap_model_t;

extern reprap_model_t reprap_model;

reprap_model_t init_reprap_model();


#endif //REPPANEL_ESP32_RRF_OBJECTS_H
