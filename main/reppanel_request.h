//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef REPPANEL_ESP32_REPPANEL_REQUEST_H
#define REPPANEL_ESP32_REPPANEL_REQUEST_H

#define MAX_REQ_ADDR_LENGTH     256 + 128
#define JSON_BUFF_SIZE          2816        // d2wc settings is max ~2600 bytes

typedef struct {
    char buffer[JSON_BUFF_SIZE];
    int buf_pos;
} response_buff_t;

void request_reprap_status_updates(void *pvParameters);

void reprap_wifi_download(response_buff_t *response_buffer, char *file);

void reprap_wifi_get_config();

void reprap_wifi_get_fileinfo(response_buff_t *resp_data, char *filename);

void reprap_wifi_get_filelist(response_buff_t *resp_buffer, char *directory);

bool reprap_wifi_send_gcode(char *gcode);

void request_filaments();

void request_macros();

void request_macros_async(char *folder_path);

void request_jobs();

void request_jobs_async(char *folder_path);

void request_fileinfo(char *file_name);

void reprap_send_gcode(char *gcode_command);

#endif //REPPANEL_ESP32_REPPANEL_REQUEST_H
