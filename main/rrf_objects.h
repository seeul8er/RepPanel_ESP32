//
// Copyright (c) 2021 Wolfgang Christl
// Licensed under Apache Licen; Version 2.0 - https//opensource.org/licenses/Apache-2.0

#ifndef REPPANEL_ESP32_RRF_OBJECTS_H
#define REPPANEL_ESP32_RRF_OBJECTS_H

#include <stdint.h>
#include <stdbool.h>
#include "reppanel.h"

#define RRF3_SEQ_BOARDS 1
#define RRF3_SEQ_DIR 2
#define RRF3_SEQ_FANS 4
#define RRF3_SEQ_GLOBAL 16
#define RRF3_SEQ_HEAT 32
#define RRF3_SEQ_INPUTS 64
#define RRF3_SEQ_JOB 128
#define RRF3_SEQ_MOVE 256
#define RRF3_SEQ_NETWORK 512
#define RRF3_SEQ_REPLY 1024
#define RRF3_SEQ_SENSORS 2048
#define RRF3_SEQ_STATE 4096
#define RRF3_SEQ_TOOLS 8192

#define REPRAP_MAX_DISPLAY_MSG_LEN     128
#define REPRAP_MAX_STATUS_LEN   15

typedef struct {
    uint16_t boards;
    uint16_t directories;
    uint16_t fans;
    uint16_t global;
    uint16_t heat ;
    uint16_t inputs ;
    uint16_t job;
    uint16_t move;
    uint16_t network;
    uint16_t reply ;
    uint16_t sensors;
    uint16_t state ;
    uint16_t tools;
} reprap_seqs_t;

typedef struct {
    uint16_t boards_changed: 1;
    uint16_t directories_changed: 1;
    uint16_t fans_changed: 1;
    uint16_t global_changed: 1;
    uint16_t heat_changed: 1;
    uint16_t inputs_changed: 1;
    uint16_t job_changed: 1;
    uint16_t move_changed: 1;
    uint16_t network_changed: 1;
    uint16_t reply_changed: 1;
    uint16_t sensors_changed: 1;
    uint16_t state_changed: 1;
    uint16_t tools_changed: 1;
} reprap_seqs_changed_t;

typedef struct {
    char disp_msg[REPRAP_MAX_DISPLAY_MSG_LEN];
    char status[REPRAP_MAX_STATUS_LEN];
} reprap_state_t;

typedef struct {
    struct {
        uint32_t size;
        char fileName[MAX_LEN_FILENAME];
        uint32_t simulatedTime;
        uint32_t printTime;
        uint16_t numLayers;
        float height;
    } file;
    uint32_t filePosition;
    uint32_t duration;
    uint32_t lastDuration;
    uint16_t layer;
    struct {
        uint32_t simulation;
        uint32_t slicer;
        uint32_t file;
    } timesLeft;
} reprap_job_t;

typedef struct {
    uint8_t api_level;
    uint8_t num_heaters;
    uint8_t num_tools;
    reprap_state_t reprap_state;
    reprap_job_t reprap_job;
    reprap_seqs_t reprap_seqs;
    reprap_seqs_changed_t reprap_seqs_changed;
} reprap_model_t;

extern reprap_model_t reprap_model;

reprap_model_t init_reprap_model();


#endif //REPPANEL_ESP32_RRF_OBJECTS_H
