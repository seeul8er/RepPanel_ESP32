//
// Copyright (c) 2020 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#ifndef REPPANEL_ESP32_REPPANEL_REQUEST_H
#define REPPANEL_ESP32_REPPANEL_REQUEST_H

#define MAX_REQ_ADDR_LENGTH     256 + 512
#define JSON_BUFF_SIZE          1024 * 4        // d2wc settings is > 2800 bytes
#define UART_RESP_BUFF_SIZE     1024 * 3

typedef struct {
    char buffer[JSON_BUFF_SIZE];
    int buf_pos;
} wifi_response_buff_t;

typedef struct {
    uint8_t buffer[UART_RESP_BUFF_SIZE];
    int buf_pos;
} uart_response_buff_t;

void request_reprap_status_updates(void *pvParameters);

void reprap_wifi_download(wifi_response_buff_t *response_buffer, char *file);

void reprap_wifi_get_config();

void reprap_wifi_get_fileinfo(wifi_response_buff_t *resp_data, char *filename);

void reprap_wifi_get_filelist(wifi_response_buff_t *resp_buffer, char *directory);

bool reprap_wifi_send_gcode(char *gcode);

void request_macros(char *folder_path);

void request_macros_async(char *folder_path);

void request_jobs(char *folder_path);

void request_jobs_async(char *folder_path);

void request_fileinfo(char *file_name);

bool reprap_send_gcode(char *gcode_command);

#endif //REPPANEL_ESP32_REPPANEL_REQUEST_H
